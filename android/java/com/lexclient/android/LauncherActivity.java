package com.lexclient.android;

import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.graphics.Color;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.util.Base64;
import android.view.View;
import android.view.Window;
import android.webkit.JavascriptInterface;
import android.webkit.WebResourceRequest;
import android.webkit.WebResourceResponse;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.util.Iterator;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * Pantalla de LeX: la misma interfaz que el launcher de PC (assets/ui), en un
 * WebView. Las llamadas de la pagina (window.lex) llegan a {@link Bridge}.
 */
public class LauncherActivity extends Activity {
    private static final String ORIGIN = "https://appassets.androidplatform.net";
    private static final int PICK_SKIN = 7;
    /** Modulos del panel nativo: nombre, descripcion, encendido de serie, disponible. */
    private static final Object[][] MODULES = {
        {"FPS", "Fotogramas por segundo", true, true},
        {"Reloj", "La hora del dispositivo", false, true},
        {"Teclas", "WASD, espacio y ratón (teclado conectado)", false, true},
        {"Coordenadas", "X Y Z del jugador", true, true},
        {"Vida", "Tu vida con corazón", true, true},
        {"Armadura", "Durabilidad de tu armadura", false, true},
        {"Velocidad", "Bloques por segundo", false, true},
        {"Rumbo", "Hacia dónde miras", false, true},
        {"Zoom", "Próximamente en Android", false, false},
        {"Brillo total", "Próximamente en Android", false, false},
        {"Física de objetos", "Próximamente en Android", false, false},
        {"Lista de jugadores", "Próximamente en Android", false, false},
    };
    private final ExecutorService worker = Executors.newSingleThreadExecutor();
    private PackInstaller installer;
    private WebView web;

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        LexLog.init(this);
        LexLog.i("LeX abierto");
        try {
            installer = new PackInstaller(this);
        } catch (Exception error) {
            LexLog.e("Catalogo de cosmeticos ilegible", error);
        }
        Window window = getWindow();
        window.setStatusBarColor(Color.rgb(10, 15, 26));
        window.setNavigationBarColor(Color.rgb(10, 15, 26));

        web = new WebView(this);
        web.setBackgroundColor(Color.rgb(10, 15, 26));
        web.setOverScrollMode(View.OVER_SCROLL_NEVER);
        WebSettings settings = web.getSettings();
        settings.setJavaScriptEnabled(true);
        settings.setDomStorageEnabled(true);
        settings.setAllowFileAccess(false);
        settings.setAllowContentAccess(false);
        settings.setMediaPlaybackRequiresUserGesture(true);
        web.addJavascriptInterface(new Bridge(), "LexNative");
        web.setWebViewClient(new WebViewClient() {
            @Override
            public WebResourceResponse shouldInterceptRequest(WebView view, WebResourceRequest request) {
                return asset(request.getUrl());
            }

            @Override
            public boolean shouldOverrideUrlLoading(WebView view, WebResourceRequest request) {
                return !request.getUrl().toString().startsWith(ORIGIN);
            }
        });
        setContentView(web);
        web.loadUrl(ORIGIN + "/ui/renderer/index.html");
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (web != null) web.evaluateJavascript("window.__lexEvent && __lexEvent('resume', null)", null);
    }

    @Override
    public void onBackPressed() {
        if (web != null && web.canGoBack()) web.goBack();
        else super.onBackPressed();
    }

    /** Los archivos de la pagina salen de los assets del APK, con su tipo. */
    private WebResourceResponse asset(Uri url) {
        if (!url.toString().startsWith(ORIGIN)) return null;
        String path = url.getPath();
        if (path == null || path.contains("..")) return null;
        path = path.substring(1);
        try {
            InputStream in = getAssets().open(path);
            return new WebResourceResponse(mime(path), mime(path).startsWith("text") ? "utf-8" : null, in);
        } catch (Exception missing) {
            WebResourceResponse none = new WebResourceResponse("text/plain", "utf-8", null);
            none.setStatusCodeAndReasonPhrase(404, "No encontrado");
            return none;
        }
    }

    private static String mime(String path) {
        if (path.endsWith(".html")) return "text/html";
        if (path.endsWith(".mjs") || path.endsWith(".js")) return "text/javascript";
        if (path.endsWith(".css")) return "text/css";
        if (path.endsWith(".json")) return "application/json";
        if (path.endsWith(".png")) return "image/png";
        return "application/octet-stream";
    }

    /** Lo que la pagina llama como window.lex.*; cada llamada en el hilo de trabajo. */
    final class Bridge {
        @JavascriptInterface
        public void call(final int id, final String name, final String args) {
            worker.execute(new Runnable() {
                @Override
                public void run() {
                    String result;
                    try {
                        Object value = handle(name, new JSONArray(args));
                        result = value == null ? "null" : value instanceof String ? JSONObject.quote((String) value) : value.toString();
                    } catch (Exception error) {
                        LexLog.e("Fallo en " + name, error);
                        String message = error.getMessage() != null ? error.getMessage() : error.toString();
                        result = "{\"__error\":" + JSONObject.quote(message) + "}";
                    }
                    final String script = "__lexDone(" + id + ", " + JSONObject.quote(result) + ")";
                    runOnUiThread(new Runnable() {
                        @Override
                        public void run() {
                            web.evaluateJavascript(script, null);
                        }
                    });
                }
            });
        }
    }

    private Object handle(String name, JSONArray args) throws Exception {
        switch (name) {
            case "status": return status();
            case "snapshot": return snapshot();
            case "files": return new JSONObject(installer.read("lex/files.json"));
            case "loadout": return loadout();
            case "saveLoadout": return saveLoadout(args.optJSONObject(0));
            case "modules": return modules();
            case "setModule": return setModule(args.getString(0), args.getBoolean(1));
            case "skin":
            case "freshSkin": return skin();
            case "play": return play();
            case "uninstall": return uninstall();
            case "shareLog": runOnUiThread(new Runnable() { @Override public void run() { shareLog(); } }); return null;
            case "pickSkin": runOnUiThread(new Runnable() { @Override public void run() { pickSkin(); } }); return null;
            default: throw new Exception("Accion desconocida: " + name);
        }
    }

    private String minecraftVersion() {
        try {
            PackageInfo info = getPackageManager().getPackageInfo(LexApplication.MINECRAFT, 0);
            return info.versionName;
        } catch (Exception missing) {
            return null;
        }
    }

    private String lexVersion() {
        try {
            return getPackageManager().getPackageInfo(getPackageName(), 0).versionName;
        } catch (Exception e) {
            return "?";
        }
    }

    private JSONObject status() throws Exception {
        String version = minecraftVersion();
        JSONObject s = new JSONObject();
        s.put("client", lexVersion());
        s.put("installed", version != null);
        s.put("version", version == null ? JSONObject.NULL : version);
        s.put("running", false);
        s.put("android", true);
        s.put("note", version != null
            ? "Minecraft " + version + ". En el juego, toca el botón LeX o pulsa L con teclado."
            : "Instala Minecraft desde la Play Store para jugar con LeX.");
        return s;
    }

    private JSONObject snapshot() throws Exception {
        JSONObject s = status();
        String version = minecraftVersion();
        boolean pack = new File(getDataDir(), "games/com.mojang/development_resource_packs/LeX_Cosmetics/manifest.json").exists();
        JSONArray rows = new JSONArray();
        rows.put(row("Minecraft", version != null ? version : "No instalado", version != null));
        rows.put(row("Dispositivo", Build.MANUFACTURER + " " + Build.MODEL + " · Android " + Build.VERSION.RELEASE, true));
        rows.put(row("Arquitectura", Build.SUPPORTED_ABIS.length > 0 ? Build.SUPPORTED_ABIS[0] : "?", Build.SUPPORTED_ABIS.length > 0 && Build.SUPPORTED_ABIS[0].equals("arm64-v8a")));
        rows.put(row("Cosméticos en el juego", pack ? "Preparados" : "Se preparan al pulsar Jugar", true));
        rows.put(row("Versión de LeX", lexVersion(), true));
        s.put("rows", rows);
        return s;
    }

    private static JSONArray row(String key, String value, boolean ok) {
        return new JSONArray().put(key).put(value).put(ok);
    }

    // ---------------------------------------------------------- equipamiento
    private File loadoutFile() {
        return new File(getFilesDir(), "loadout.json");
    }

    private JSONObject loadout() throws Exception {
        File file = loadoutFile();
        JSONObject saved = file.exists() ? new JSONObject(new String(readAll(new FileInputStream(file)), StandardCharsets.UTF_8)) : new JSONObject();
        // Solo lo que existe en este APK.
        JSONObject clean = new JSONObject();
        Iterator<String> keys = saved.keys();
        while (keys.hasNext()) {
            String slot = keys.next();
            String id = saved.optString(slot, null);
            if (id != null && !saved.isNull(slot) && installer != null && installer.item(id) != null) clean.put(slot, id);
        }
        return clean;
    }

    private JSONObject saveLoadout(JSONObject value) throws Exception {
        JSONObject clean = new JSONObject();
        if (value != null) {
            Iterator<String> keys = value.keys();
            while (keys.hasNext()) {
                String slot = keys.next();
                if (value.isNull(slot)) continue;
                String id = value.getString(slot);
                if (installer != null && installer.item(id) != null) clean.put(slot, id);
            }
        }
        write(loadoutFile(), clean.toString(2).getBytes(StandardCharsets.UTF_8));
        return clean;
    }

    private Map<String, String> loadoutMap() throws Exception {
        JSONObject json = loadout();
        Map<String, String> out = new LinkedHashMap<>();
        Iterator<String> keys = json.keys();
        while (keys.hasNext()) {
            String slot = keys.next();
            out.put(slot, json.getString(slot));
        }
        return out;
    }

    // ---------------------------------------------------------- modulos
    private File modulesFile() {
        return new File(getFilesDir(), "android_modules.json");
    }

    private JSONArray modules() throws Exception {
        File file = modulesFile();
        JSONObject saved = file.exists() ? new JSONObject(new String(readAll(new FileInputStream(file)), StandardCharsets.UTF_8)) : new JSONObject();
        JSONArray list = new JSONArray();
        for (Object[] m : MODULES) {
            String name = (String) m[0];
            boolean available = (Boolean) m[3];
            JSONObject o = new JSONObject();
            o.put("name", name);
            o.put("description", m[1]);
            o.put("category", available ? "En pantalla" : "Próximamente");
            o.put("enabled", available && saved.optBoolean(name, (Boolean) m[2]));
            list.put(o);
        }
        return list;
    }

    private boolean setModule(String name, boolean on) throws Exception {
        for (Object[] m : MODULES) {
            if (!m[0].equals(name)) continue;
            if (!(Boolean) m[3]) throw new Exception(name + " llegará pronto a Android");
            File file = modulesFile();
            JSONObject saved = file.exists() ? new JSONObject(new String(readAll(new FileInputStream(file)), StandardCharsets.UTF_8)) : new JSONObject();
            saved.put(name, on);
            write(file, saved.toString(2).getBytes(StandardCharsets.UTF_8));
            return true;
        }
        throw new Exception("Módulo desconocido");
    }

    // ---------------------------------------------------------- skin
    private File skinFile() {
        return new File(getFilesDir(), "skin.png");
    }

    /** La skin elegida o, si no, la ultima que guardo el juego dentro de LeX. */
    private String skin() throws Exception {
        File chosen = skinFile();
        File best = chosen.exists() ? chosen : null;
        if (best == null && installer != null) {
            long time = 0;
            for (File dir : installer.gameDirs()) {
                File[] list = dir == null ? null : new File(dir, "custom_skins").listFiles();
                for (int i = 0; list != null && i < list.length; i++) {
                    if (list[i].getName().endsWith(".png") && list[i].lastModified() > time) {
                        time = list[i].lastModified();
                        best = list[i];
                    }
                }
            }
        }
        if (best == null) return null;
        return "data:image/png;base64," + Base64.encodeToString(readAll(new FileInputStream(best)), Base64.NO_WRAP);
    }

    private void pickSkin() {
        Intent pick = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        pick.addCategory(Intent.CATEGORY_OPENABLE);
        pick.setType("image/png");
        try {
            startActivityForResult(pick, PICK_SKIN);
        } catch (Exception error) {
            LexLog.e("No se pudo abrir el selector", error);
        }
    }

    @Override
    protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        if (request != PICK_SKIN || result != RESULT_OK || data == null || data.getData() == null) return;
        final Uri uri = data.getData();
        worker.execute(new Runnable() {
            @Override
            public void run() {
                try {
                    byte[] png = readAll(getContentResolver().openInputStream(uri));
                    if (png.length < 24) throw new Exception("La skin debe ser un PNG de 64x64");
                    // 64x64 o 64x32 (cabecera PNG: ancho y alto en los bytes 16-23).
                    int w = ((png[16] & 255) << 24) | ((png[17] & 255) << 16) | ((png[18] & 255) << 8) | (png[19] & 255);
                    if (w != 64 && w != 128) throw new Exception("La skin debe ser un PNG de 64x64");
                    write(skinFile(), png);
                    final String url = "data:image/png;base64," + Base64.encodeToString(png, Base64.NO_WRAP);
                    runOnUiThread(new Runnable() {
                        @Override
                        public void run() {
                            web.evaluateJavascript("__lexEvent('skin', " + JSONObject.quote(url) + ")", null);
                        }
                    });
                } catch (Exception error) {
                    LexLog.e("Skin no valida", error);
                }
            }
        });
    }

    // ---------------------------------------------------------- jugar
    private JSONObject play() throws Exception {
        if (minecraftVersion() == null) throw new Exception("Instala Minecraft desde la Play Store primero.");
        if (installer == null) throw new Exception("El APK de LeX está dañado: reinstálalo.");
        installer.install(loadoutMap());
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                Intent game = new Intent().setClassName(LauncherActivity.this, "com.mojang.minecraftpe.MainActivity");
                game.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                LexLog.i("Arrancando Minecraft dentro de LeX");
                startActivity(game);
            }
        });
        return new JSONObject().put("ok", true).put("message", "Abriendo Minecraft con LeX…");
    }

    private JSONObject uninstall() throws Exception {
        if (installer != null) installer.uninstall();
        return new JSONObject().put("ok", true).put("message", "Cosméticos de LeX quitados del juego.");
    }

    private void shareLog() {
        Intent send = new Intent(Intent.ACTION_SEND);
        send.setType("text/plain");
        send.putExtra(Intent.EXTRA_STREAM, Uri.parse("content://com.lexclient.android.logs/lex.log"));
        send.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        startActivity(Intent.createChooser(send, "Compartir registro de LeX"));
    }

    // ---------------------------------------------------------- archivos
    static byte[] readAll(InputStream in) throws Exception {
        try (InputStream input = in) {
            ByteArrayOutputStream out = new ByteArrayOutputStream();
            byte[] buffer = new byte[65536];
            for (int n; (n = input.read(buffer)) > 0; ) out.write(buffer, 0, n);
            return out.toByteArray();
        }
    }

    static void write(File file, byte[] data) throws Exception {
        file.getParentFile().mkdirs();
        try (OutputStream out = new FileOutputStream(file)) {
            out.write(data);
        }
    }
}
