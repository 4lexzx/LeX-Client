package com.lexclient.android;

import android.content.Context;
import android.content.res.AssetManager;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.util.HashMap;
import java.util.Map;

/**
 * Instala el pack de LeX donde el juego lo lee cuando corre dentro de esta app.
 * En PC la DLL pinta en vivo la textura de cosmeticos; aqui se compone al
 * pulsar Jugar con lo que llevas puesto.
 */
final class PackInstaller {
    static final int DENSITY = 12;
    private final Context context;
    private final AssetManager assets;
    final JSONObject catalog;

    PackInstaller(Context context) throws Exception {
        this.context = context;
        this.assets = context.getAssets();
        this.catalog = new JSONObject(read("lex/catalog.json"));
    }

    String read(String path) throws Exception {
        try (InputStream in = assets.open(path)) {
            ByteArrayOutputStream out = new ByteArrayOutputStream();
            byte[] buffer = new byte[65536];
            for (int n; (n = in.read(buffer)) > 0; ) out.write(buffer, 0, n);
            return out.toString(StandardCharsets.UTF_8.name());
        }
    }

    JSONObject item(String id) throws Exception {
        JSONArray items = catalog.getJSONArray("items");
        for (int i = 0; i < items.length(); i++) {
            if (items.getJSONObject(i).getString("id").equals(id)) return items.getJSONObject(i);
        }
        return null;
    }

    /** Carpetas com.mojang donde puede leer el juego dentro de LeX. */
    File[] gameDirs() {
        File external = context.getExternalFilesDir(null);
        return new File[] {
            new File(context.getDataDir(), "games/com.mojang"),
            external != null ? new File(external, "games/com.mojang") : null,
        };
    }

    void install(Map<String, String> loadout) throws Exception {
        String kill = key(loadout.get("kill"), "kill-");
        String death = key(loadout.get("death"), "death-");
        Map<String, byte[]> files = new HashMap<>();
        collect("lex/pack", "", files);
        collect("lex/combos/" + kill + "__" + death, "", files);
        files.put("textures/lex/atlas.png", png(atlas(loadout)));
        channels(loadout, files);
        String manifest = new String(files.get("manifest.json"), StandardCharsets.UTF_8);
        JSONObject header = new JSONObject(manifest).getJSONObject("header");
        String packs = "[{\"pack_id\":\"" + header.getString("uuid") + "\",\"version\":" + header.getJSONArray("version") + "}]";
        for (File dir : gameDirs()) {
            if (dir == null) continue;
            File root = new File(dir, "development_resource_packs/LeX_Cosmetics");
            delete(root);
            for (Map.Entry<String, byte[]> f : files.entrySet()) write(new File(root, f.getKey()), f.getValue());
            write(new File(dir, "minecraftpe/global_resource_packs.json"), packs.getBytes(StandardCharsets.UTF_8));
        }
        LexLog.i("Pack de LeX instalado (" + files.size() + " archivos, baja " + kill + ", muerte " + death + ")");
    }

    /** Carpeta files/panel: lo que el panel nativo dibuja dentro del juego. */
    File panelDir() {
        File dir = new File(context.getFilesDir(), "panel");
        dir.mkdirs();
        return dir;
    }

    /** Catalogo, logo e iconos de los mods para el panel dentro del juego. */
    void writePanel() throws Exception {
        File dir = panelDir();
        write(new File(dir, "catalog.json"), read("lex/catalog.json").getBytes(StandardCharsets.UTF_8));
        rgba(new File(dir, "logo.rgba"), picture("ui/renderer/logo-mark.png"), 160);
        String[] icons = assets.list("ui/renderer/icons");
        for (int i = 0; icons != null && i < icons.length; i++) {
            if (!icons[i].endsWith(".png")) continue;
            File out = new File(dir, "icon-" + icons[i].substring(0, icons[i].length() - 4) + ".rgba");
            if (!out.exists()) rgba(out, picture("ui/renderer/icons/" + icons[i]), 96);
        }
    }

    /** Imagen normal (premultiplicada): se puede escalar con Canvas. */
    private Bitmap picture(String path) {
        try (InputStream in = assets.open(path)) {
            return BitmapFactory.decodeStream(in);
        } catch (Exception missing) {
            return null;
        }
    }

    /**
     * RGBA crudo para el panel nativo: ancho y alto (int32 little-endian) y
     * los pixeles sin premultiplicar, a lo sumo de max px de lado.
     */
    static void rgba(File file, Bitmap bitmap, int max) throws Exception {
        if (bitmap == null) return;
        int w = bitmap.getWidth(), h = bitmap.getHeight();
        if (Math.max(w, h) > max) {
            float k = max / (float) Math.max(w, h);
            bitmap = Bitmap.createScaledBitmap(bitmap, Math.max(1, Math.round(w * k)), Math.max(1, Math.round(h * k)), true);
            w = bitmap.getWidth();
            h = bitmap.getHeight();
        }
        int[] pixels = new int[w * h];
        bitmap.getPixels(pixels, 0, w, 0, 0, w, h);
        byte[] out = new byte[8 + pixels.length * 4];
        java.nio.ByteBuffer.wrap(out, 0, 8).order(java.nio.ByteOrder.LITTLE_ENDIAN).putInt(w).putInt(h);
        for (int i = 0, o = 8; i < pixels.length; i++, o += 4) {
            int c = pixels[i];
            out[o] = (byte) (c >> 16);
            out[o + 1] = (byte) (c >> 8);
            out[o + 2] = (byte) c;
            out[o + 3] = (byte) (c >>> 24);
        }
        write(file, out);
    }

    /** Quita el pack de LeX de las carpetas del juego. */
    void uninstall() {
        for (File dir : gameDirs()) {
            if (dir == null) continue;
            delete(new File(dir, "development_resource_packs/LeX_Cosmetics"));
            new File(dir, "minecraftpe/global_resource_packs.json").delete();
        }
        LexLog.i("Pack de LeX quitado");
    }

    private static String key(String id, String prefix) {
        return id != null && id.startsWith(prefix) ? id.substring(prefix.length()) : "none";
    }

    private void collect(String base, String sub, Map<String, byte[]> out) throws Exception {
        String path = sub.isEmpty() ? base : base + "/" + sub;
        String[] list = assets.list(path);
        if (list == null || list.length == 0) {
            if (sub.isEmpty()) return;
            try (InputStream in = assets.open(path)) {
                ByteArrayOutputStream data = new ByteArrayOutputStream();
                byte[] buffer = new byte[65536];
                for (int n; (n = in.read(buffer)) > 0; ) data.write(buffer, 0, n);
                out.put(sub, data.toByteArray());
            }
            return;
        }
        for (String name : list) collect(base, sub.isEmpty() ? name : sub + "/" + name, out);
    }

    /** La textura de cosmeticos con lo que llevas puesto (primer fotograma). */
    private Bitmap atlas(Map<String, String> loadout) throws Exception {
        JSONArray size = catalog.getJSONArray("atlas");
        Bitmap atlas = blank(size.getInt(0) * DENSITY, size.getInt(1) * DENSITY);
        for (String id : loadout.values()) {
            JSONObject item = id == null ? null : item(id);
            if (item == null || item.has("trail")) continue;
            Bitmap art = image("lex/cosmetics/" + id + ".png");
            if (art == null) continue;
            JSONArray r = item.getJSONArray("rect");
            blit(atlas, art, r.getInt(0) * DENSITY, r.getInt(1) * DENSITY, r.getInt(2) * DENSITY, r.getInt(3) * DENSITY);
            art.recycle();
        }
        return atlas;
    }

    /** Particulas del rastro y de la mascota que llevas: su dibujo en su canal. */
    private void channels(Map<String, String> loadout, Map<String, byte[]> files) throws Exception {
        for (String id : loadout.values()) {
            JSONObject item = id == null ? null : item(id);
            if (item == null) continue;
            if (item.has("trail") && item.getInt("trail") >= 0) {
                int channel = item.getInt("trail");
                Bitmap art = image("lex/cosmetics/" + id + ".png");
                if (art == null) continue;
                Bitmap sheet = blank(264, 40 + channel);
                blit(sheet, art, 0, 0, 264, 40 + channel);
                files.put("textures/lex/p/" + channel + ".png", png(sheet));
            }
            JSONArray particles = item.optJSONArray("particles");
            for (int i = 0; particles != null && i < particles.length(); i++) {
                JSONObject p = particles.getJSONObject(i);
                Bitmap art = image("lex/cosmetics/" + p.getString("file") + ".png");
                if (art != null) files.put("textures/lex/fp/" + p.getInt("channel") + ".png", png(art));
            }
        }
    }

    /**
     * Lienzo sin premultiplicar: los colores semitransparentes se guardan tal
     * cual (Canvas no acepta bitmaps sin premultiplicar, por eso se copian
     * los pixeles a mano).
     */
    private static Bitmap blank(int w, int h) {
        Bitmap bitmap = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888);
        bitmap.setPremultiplied(false);
        return bitmap;
    }

    private static void blit(Bitmap dst, Bitmap src, int x, int y, int w, int h) {
        w = Math.min(w, Math.min(src.getWidth(), dst.getWidth() - x));
        h = Math.min(h, Math.min(src.getHeight(), dst.getHeight() - y));
        if (w <= 0 || h <= 0) return;
        int[] pixels = new int[w * h];
        src.getPixels(pixels, 0, w, 0, 0, w, h);
        dst.setPixels(pixels, 0, w, x, y, w, h);
    }

    private Bitmap image(String path) {
        try (InputStream in = assets.open(path)) {
            BitmapFactory.Options options = new BitmapFactory.Options();
            options.inScaled = false;
            options.inPremultiplied = false;
            return BitmapFactory.decodeStream(in, null, options);
        } catch (Exception missing) {
            return null;
        }
    }

    private static byte[] png(Bitmap bitmap) {
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        bitmap.compress(Bitmap.CompressFormat.PNG, 100, out);
        return out.toByteArray();
    }

    private static void write(File file, byte[] data) throws Exception {
        file.getParentFile().mkdirs();
        try (OutputStream out = new FileOutputStream(file)) {
            out.write(data);
        }
    }

    private static void delete(File file) {
        File[] children = file.listFiles();
        if (children != null) for (File child : children) delete(child);
        file.delete();
    }
}
