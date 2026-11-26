package com.lexclient.android;

import android.content.Context;
import android.util.Log;

import java.io.File;
import java.io.FileWriter;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

/**
 * Registro de LeX. Lo escriben los dos procesos (launcher y juego) y la parte
 * nativa en el mismo archivo, para poder enviarlo desde "Compartir registro".
 */
public final class LexLog {
    private static final String TAG = "LeX";
    private static File file;

    private LexLog() {}

    public static synchronized void init(Context context) {
        if (file != null) return;
        File dir = context.getExternalFilesDir(null);
        if (dir == null) dir = context.getFilesDir();
        dir.mkdirs();
        file = new File(dir, "lex.log");
        // Que no crezca sin fin: se reinicia al pasar de 1 MB.
        if (file.length() > 1024 * 1024) file.delete();
    }

    public static File file() {
        return file;
    }

    public static synchronized void i(String message) {
        Log.i(TAG, message);
        write("I", message);
    }

    public static synchronized void e(String message, Throwable error) {
        Log.e(TAG, message, error);
        StringWriter trace = new StringWriter();
        if (error != null) error.printStackTrace(new PrintWriter(trace));
        write("E", message + (error != null ? "\n" + trace : ""));
    }

    private static void write(String level, String message) {
        if (file == null) return;
        try (FileWriter out = new FileWriter(file, true)) {
            String time = new SimpleDateFormat("HH:mm:ss", Locale.US).format(new Date());
            out.write("[" + time + "] [" + level + "] [java:" + android.os.Process.myPid() + "] " + message + "\n");
        } catch (Exception ignored) {
            // sin almacenamiento: solo logcat
        }
    }
}
