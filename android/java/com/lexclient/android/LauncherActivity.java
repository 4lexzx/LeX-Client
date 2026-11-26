package com.lexclient.android;

import android.app.Activity;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageInfo;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.net.Uri;
import android.os.Bundle;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.Spinner;
import android.widget.TextView;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.File;
import java.io.FileWriter;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/** Pantalla de LeX: estado, equipamiento, Jugar y Compartir registro. */
public class LauncherActivity extends Activity {
    private static final int BG = Color.rgb(9, 13, 22);
    private static final int CARD = Color.rgb(18, 26, 42);
    private static final int ACCENT = Color.rgb(47, 155, 255);
    private static final String[][] SLOTS = {
        {"cape", "Capa"}, {"wings", "Alas"}, {"head", "Cabeza"}, {"pet", "Mascota"}, {"aura", "Aura"},
        {"trail", "Partículas"}, {"hit", "Golpes"}, {"kill", "Kills"}, {"death", "Muertes"},
    };
    private final Map<String, String> loadout = new LinkedHashMap<>();
    private PackInstaller installer;
    private TextView status;

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        LexLog.init(this);
        LexLog.i("LeX abierto");
        final SharedPreferences prefs = getSharedPreferences("loadout", MODE_PRIVATE);
        try {
            installer = new PackInstaller(this);
        } catch (Exception error) {
            LexLog.e("Catalogo de cosmeticos ilegible", error);
        }

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setBackgroundColor(BG);
        int pad = dp(18);
        root.setPadding(pad, pad, pad, pad);

        TextView title = text("LeX Client", 30, Color.WHITE);
        title.setTypeface(Typeface.DEFAULT_BOLD);
        root.addView(title);
        status = text(minecraftStatus(), 14, Color.rgb(150, 170, 200));
        root.addView(status);

        LinearLayout slots = new LinearLayout(this);
        slots.setOrientation(LinearLayout.VERTICAL);
        for (String[] slot : SLOTS) slots.addView(slotRow(slot[0], slot[1], prefs));
        ScrollView scroll = new ScrollView(this);
        scroll.addView(slots);
        root.addView(scroll, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1));

        LinearLayout buttons = new LinearLayout(this);
        buttons.setGravity(Gravity.END);
        Button share = button("Compartir registro", CARD);
        share.setOnClickListener(new View.OnClickListener() { @Override public void onClick(View v) { shareLog(); } });
        Button play = button("Jugar", ACCENT);
        play.setOnClickListener(new View.OnClickListener() { @Override public void onClick(View v) { play(prefs); } });
        buttons.addView(share);
        buttons.addView(play);
        root.addView(buttons);
        setContentView(root);
    }

    private String minecraftStatus() {
        try {
            PackageInfo info = getPackageManager().getPackageInfo(LexApplication.MINECRAFT, 0);
            return "Minecraft " + info.versionName + " instalado · panel: botón LeX o tecla L";
        } catch (Exception missing) {
            return "Instala Minecraft desde la Play Store para jugar con LeX.";
        }
    }

    private View slotRow(final String slot, String label, final SharedPreferences prefs) {
        LinearLayout row = new LinearLayout(this);
        row.setGravity(Gravity.CENTER_VERTICAL);
        row.setPadding(dp(12), dp(6), dp(12), dp(6));
        GradientDrawable bg = new GradientDrawable();
        bg.setColor(CARD);
        bg.setCornerRadius(dp(10));
        row.setBackground(bg);
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        lp.topMargin = dp(8);
        row.setLayoutParams(lp);
        TextView name = text(label, 16, Color.WHITE);
        row.addView(name, new LinearLayout.LayoutParams(dp(120), ViewGroup.LayoutParams.WRAP_CONTENT));

        final List<String> ids = new ArrayList<>();
        List<String> names = new ArrayList<>();
        ids.add(null);
        names.add("Nada");
        try {
            JSONArray items = installer.catalog.getJSONArray("items");
            for (int i = 0; i < items.length(); i++) {
                JSONObject item = items.getJSONObject(i);
                if (!item.getString("slot").equals(slot)) continue;
                ids.add(item.getString("id"));
                names.add(item.getString("name"));
            }
        } catch (Exception ignored) {
            // sin catalogo: solo "Nada"
        }
        Spinner spinner = new Spinner(this);
        ArrayAdapter<String> adapter = new ArrayAdapter<>(this, android.R.layout.simple_spinner_dropdown_item, names);
        spinner.setAdapter(adapter);
        String current = prefs.getString(slot, null);
        int at = ids.indexOf(current);
        spinner.setSelection(Math.max(0, at));
        loadout.put(slot, at > 0 ? current : null);
        spinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                loadout.put(slot, ids.get(position));
                prefs.edit().putString(slot, ids.get(position)).apply();
            }

            @Override public void onNothingSelected(AdapterView<?> parent) {}
        });
        row.addView(spinner, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1));
        return row;
    }

    private void play(SharedPreferences prefs) {
        status.setText("Preparando LeX…");
        final Activity self = this;
        new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    if (installer != null) installer.install(loadout);
                    writeLoadout();
                    runOnUiThread(new Runnable() {
                        @Override
                        public void run() {
                            Intent game = new Intent().setClassName(self, "com.mojang.minecraftpe.MainActivity");
                            game.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                            LexLog.i("Arrancando Minecraft dentro de LeX");
                            startActivity(game);
                            status.setText(minecraftStatus());
                        }
                    });
                } catch (final Exception error) {
                    LexLog.e("No se pudo preparar el juego", error);
                    runOnUiThread(new Runnable() {
                        @Override
                        public void run() {
                            status.setText("Error al preparar el juego: " + error.getMessage());
                        }
                    });
                }
            }
        }).start();
    }

    /** Lo que llevas puesto, para el panel nativo dentro del juego. */
    private void writeLoadout() throws Exception {
        JSONObject json = new JSONObject();
        for (Map.Entry<String, String> e : loadout.entrySet()) json.put(e.getKey(), e.getValue() == null ? JSONObject.NULL : e.getValue());
        try (FileWriter out = new FileWriter(new File(getFilesDir(), "loadout.json"))) {
            out.write(json.toString(2));
        }
    }

    private void shareLog() {
        Intent send = new Intent(Intent.ACTION_SEND);
        send.setType("text/plain");
        send.putExtra(Intent.EXTRA_STREAM, Uri.parse("content://com.lexclient.android.logs/lex.log"));
        send.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        startActivity(Intent.createChooser(send, "Compartir registro de LeX"));
    }

    private TextView text(String value, int size, int color) {
        TextView view = new TextView(this);
        view.setText(value);
        view.setTextSize(size);
        view.setTextColor(color);
        return view;
    }

    private Button button(String label, int color) {
        Button view = new Button(this);
        view.setText(label);
        view.setTextColor(Color.WHITE);
        view.setAllCaps(false);
        GradientDrawable bg = new GradientDrawable();
        bg.setColor(color);
        bg.setCornerRadius(dp(10));
        view.setBackground(bg);
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        lp.leftMargin = dp(10);
        lp.topMargin = dp(10);
        view.setLayoutParams(lp);
        view.setPadding(dp(22), dp(10), dp(22), dp(10));
        return view;
    }

    private int dp(int value) {
        return Math.round(value * getResources().getDisplayMetrics().density);
    }
}
