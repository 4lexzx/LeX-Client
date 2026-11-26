package com.lexclient.android;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.provider.OpenableColumns;

import java.io.File;
import java.io.FileNotFoundException;

/**
 * Entrega ficheros de la carpeta files/share por su nombre, para pasarlos a
 * otra app con un content:// (por ejemplo el .mcpack que importa Minecraft).
 */
public class ShareProvider extends ContentProvider {
    static final String AUTHORITY = "com.lexclient.android.share";

    static Uri uriFor(String name) {
        return Uri.parse("content://" + AUTHORITY + "/" + name);
    }

    @Override public boolean onCreate() { return true; }

    private File file(Uri uri) {
        String name = uri.getLastPathSegment();
        if (name == null || name.contains("..") || name.contains("/")) return null;
        return new File(new File(getContext().getFilesDir(), "share"), name);
    }

    @Override
    public ParcelFileDescriptor openFile(Uri uri, String mode) throws FileNotFoundException {
        File file = file(uri);
        if (file == null || !file.exists()) throw new FileNotFoundException("No existe " + uri);
        return ParcelFileDescriptor.open(file, ParcelFileDescriptor.MODE_READ_ONLY);
    }

    @Override
    public Cursor query(Uri uri, String[] projection, String selection, String[] args, String order) {
        File file = file(uri);
        MatrixCursor cursor = new MatrixCursor(new String[] {OpenableColumns.DISPLAY_NAME, OpenableColumns.SIZE});
        cursor.addRow(new Object[] {uri.getLastPathSegment(), file != null && file.exists() ? file.length() : 0});
        return cursor;
    }

    @Override
    public String getType(Uri uri) {
        return "application/octet-stream";
    }

    @Override public Uri insert(Uri uri, ContentValues values) { return null; }
    @Override public int delete(Uri uri, String selection, String[] args) { return 0; }
    @Override public int update(Uri uri, ContentValues values, String selection, String[] args) { return 0; }
}
