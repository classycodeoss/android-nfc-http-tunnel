package com.classycode.nfchttptun;

import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.v4.app.DialogFragment;
import android.support.v7.app.AlertDialog;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.EditText;

import java.net.MalformedURLException;
import java.net.URL;

/**
 * Dialog to input an URL. Performs validation by attempting to parse the URL.
 *
 * @author Alex Suzuki, Classy Code GmbH, 2017
 */
public class SetUrlDialogFragment extends DialogFragment {

    private static final String TAG = Constants.LOG_TAG;

    public interface SetUrlDialogListener {

        void onSetUrlDialogSucceeded(String urlSpec);

        void onSetUrlDialogCancelled();
    }

    private SetUrlDialogListener listener;

    @NonNull
    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        final LayoutInflater inflater = getActivity().getLayoutInflater();
        final AlertDialog.Builder builder = new AlertDialog.Builder(getActivity());
        final View contentView = inflater.inflate(R.layout.dialog_set_url, null);
        final EditText urlField = (EditText) contentView.findViewById(R.id.url_field);
        urlField.setText(TunnelSettings.getUrl(getContext()));
        builder.setView(contentView)
                .setPositiveButton(android.R.string.ok, new DialogInterface.OnClickListener() {

                    @Override
                    public void onClick(DialogInterface dialog, int id) {
                        final String urlSpec = urlField.getText().toString();
                        try {
                            final URL parsedUrl = new URL(urlSpec);
                            listener.onSetUrlDialogSucceeded(parsedUrl.toString());
                        } catch (MalformedURLException e) {
                            Log.e(TAG, "Ignoring malformed URL: " + urlSpec, e);
                        }
                    }
                })
                .setNegativeButton(android.R.string.cancel, new DialogInterface.OnClickListener() {

                    @Override
                    public void onClick(DialogInterface dialog, int id) {

                        listener.onSetUrlDialogCancelled();
                    }
                });
        // Create the AlertDialog object and return it
        return builder.create();
    }

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);
        listener = (SetUrlDialogListener) context;
    }

}
