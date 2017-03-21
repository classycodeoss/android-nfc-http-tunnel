#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <nfc/nfc.h>
#include <curl/curl.h>

// By default we expect the PN532 to be connected via SPI
#define DEFAULT_CONN_STRING "pn532_spi:/dev/spidev0.0:1000000"

// Maximum payload size for APDUs in the data transmission. You might have to lower this for your phone.
#define DATA_PACKET_SIZE 250

// Default timeout for APDU transmissions
#define TX_TIMEOUT 1000

struct data_buffer {
    char *ptr;
    size_t size;
};

void print_hex(FILE *fd, const uint8_t *data, const size_t len) {
    size_t pos;
    for (pos = 0; pos < len; pos++) {
        fprintf(fd, "%02x", data[pos]);
    }
}

int card_transmit(nfc_device *pnd, uint8_t *capdu, size_t capdulen, uint8_t *rapdu, size_t *rapdulen) {
    int res;
    printf("=> ");
    print_hex(stdout, capdu, capdulen);
    printf("\n");
    if ((res = nfc_initiator_transceive_bytes(pnd, capdu, capdulen, rapdu, *rapdulen, TX_TIMEOUT)) < 0) {
        return -1;
    } else {
        *rapdulen = (size_t) res;
        printf("<= ");
        print_hex(stdout, rapdu, res);
        printf("\n");
        return 0;
    }
}

static size_t curl_writecb(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct data_buffer *buffer = (struct data_buffer *) userp;
    buffer->ptr = realloc(buffer->ptr, buffer->size + realsize + 1);
    if (buffer->ptr == NULL) { // out of memory
        fprintf(stderr, "not enough memory (realloc returned NULL)\n");
        return 0;
    }
    memcpy(&(buffer->ptr[buffer->size]), contents, realsize);
    buffer->size += realsize;
    buffer->ptr[buffer->size] = 0;
    return realsize;
}

// Download URL into given in-memory buffer, inspired by https://curl.haxx.se/libcurl/c/getinmemory.html
int download_url(const char *url, struct data_buffer *buffer) {
    CURL *curl_sess = curl_easy_init();
    if (curl_sess) {
        CURLcode res;
        curl_easy_setopt(curl_sess, CURLOPT_URL, url);
        curl_easy_setopt(curl_sess, CURLOPT_WRITEFUNCTION, curl_writecb);
        curl_easy_setopt(curl_sess, CURLOPT_WRITEDATA, (void *) buffer);
        curl_easy_setopt(curl_sess, CURLOPT_USERAGENT, "pn532-http-tunnel/1.0");
        curl_easy_setopt(curl_sess, CURLOPT_FOLLOWLOCATION, 1L);
        res = curl_easy_perform(curl_sess);
        curl_easy_cleanup(curl_sess);
        return res == CURLE_OK ? 0 : res;
    } else {
        fprintf(stderr, "Failed to initialize curl\n");
        return -1;
    }
}

int main(int argc, const char *argv[]) {

    // initialize CURL
    curl_global_init(CURL_GLOBAL_ALL);

    // initialize NFC reader
    const char *lib_version = nfc_version();
    printf("Using libnfc version: %s\n", lib_version);

    nfc_device *pnd = NULL;
    nfc_context *context = NULL;

    nfc_init(&context);
    if (!context) {
        fprintf(stderr, "Unable to initialize nfc (nfc_init failed)\n");
        exit(EXIT_FAILURE);
    }

    const char *conn_string = argc > 1 ? argv[1] : DEFAULT_CONN_STRING;
    pnd = nfc_open(context, conn_string);
    if (!pnd) {
        fprintf(stderr, "Unable to open device: %s\n", conn_string);
        exit(EXIT_FAILURE);
    }

    if (nfc_initiator_init(pnd) < 0) {
        fprintf(stderr, "nfc_initiator_init failed: %s\n", nfc_strerror(pnd));
        exit(EXIT_FAILURE);
    }

    printf("NFC device opened: %s\n", nfc_device_get_name(pnd));

    // Poll for a ISO14443A (MIFARE) tag
    const nfc_modulation mod_mifare = {
            .nmt = NMT_ISO14443A,
            .nbr = NBR_106,
    };
    nfc_target target;

    while (true) {

        printf("Waiting for device...\n");
        if (nfc_initiator_select_passive_target(pnd, mod_mifare, NULL, 0, &target) > 0) {
            uint8_t command_apdu[512];
            uint8_t response_apdu[512];
            size_t command_apdu_len;
            size_t response_apdu_len;

            // send selection application APDU
            command_apdu_len = 11;
            response_apdu_len = sizeof(response_apdu);
            memcpy(command_apdu, "\x00\xA4\x04\x00\x06\xF0\xAB\xCD\xEF\x00\x00", command_apdu_len);
            if (card_transmit(pnd, command_apdu, command_apdu_len, response_apdu, &response_apdu_len) < 0) {
                fprintf(stderr, "SELECT AID APDU transmission failed: %s\n", nfc_strerror(pnd));
                goto cleanup;
            }
            if (response_apdu_len < 2 || response_apdu[response_apdu_len - 2] != 0x90 ||
                response_apdu[response_apdu_len - 1] != 0x00) {
                fprintf(stderr, "Malformed SELECT AID APDU response\n");
                goto cleanup;
            }

            // application selected, now ask for URL to download
            command_apdu_len = 1;
            command_apdu[0] = 0x01; // READ_URL
            response_apdu_len = sizeof(response_apdu);
            if (card_transmit(pnd, command_apdu, command_apdu_len, response_apdu, &response_apdu_len) < 0) {
                fprintf(stderr, "READ_URL command transmission failed: %s\n", nfc_strerror(pnd));
                goto cleanup;
            }

            // response has format: STATUS::URL(ASCII)
            if (response_apdu_len < 1 || response_apdu[0] != 0x00) {
                fprintf(stderr, "Malformed READ_URL response\n");
                goto cleanup;
            }

            char url[response_apdu_len]; // including null terminator
            memcpy(url, response_apdu + 1, response_apdu_len - 1);
            url[response_apdu_len - 1] = 0;
            printf("Received URL for download: %s\n", url);

            struct data_buffer buffer;
            buffer.ptr = malloc(1);
            buffer.size = 0;
            int res = download_url(url, &buffer);
            if (res == 0) {

                // calculate number of packets
                unsigned short num_packets = buffer.size / DATA_PACKET_SIZE;
                if (buffer.size % DATA_PACKET_SIZE != 0) {
                    num_packets++;
                }
                printf("Attempting to send data (%ld bytes) as %d packets (packet size: %d bytes)\n",
                       buffer.size, num_packets, DATA_PACKET_SIZE);

                const char *cur = buffer.ptr;
                for (unsigned short seq_no = 0; seq_no < num_packets; seq_no++) {
                    int payload_len;
                    if (seq_no == num_packets - 1) {
                        payload_len = buffer.size % DATA_PACKET_SIZE; // last packet
                    } else {
                        payload_len = DATA_PACKET_SIZE;
                    }
                    memcpy(command_apdu + 5, cur, payload_len);
                    command_apdu_len = 5 + payload_len;
                    command_apdu[0] = 0x02; // DATA

                    command_apdu[1] = (seq_no >> 8) & 0xff; // SEQ NO MSB
                    command_apdu[2] = seq_no & 0xff; // SEQ NO LSB
                    command_apdu[3] = (num_packets >> 8) & 0xff; // MSB total packets
                    command_apdu[4] = num_packets & 0xff; // LSB total packets

                    response_apdu_len = sizeof(response_apdu);
                    if (card_transmit(pnd, command_apdu, command_apdu_len, response_apdu, &response_apdu_len) < 0) {
                        fprintf(stderr, "DATA COMMAND transmission failed: %s\n", nfc_strerror(pnd));
                        free(buffer.ptr);
                        goto cleanup;
                    }

                    // check response
                    if (response_apdu_len < 1 || response_apdu[0] != 0x0) {
                        fprintf(stderr, "Malformed response to data transmission");
                        free(buffer.ptr);
                        goto cleanup;
                    }

                    // advance to next packet
                    cur += DATA_PACKET_SIZE;
                }

                // free the buffer
                free(buffer.ptr);
            } else {
                // TODO: notify device
                fprintf(stderr, "URL download failed with error code: %d\n", res);
                goto cleanup;
            }
        }

        cleanup:
        printf("Transaction ended, cleaning up...\n");
        nfc_initiator_deselect_target(pnd);
    }

    // clean up
    nfc_close(pnd);
    nfc_exit(context);

    exit(EXIT_SUCCESS);
}




