package com.lexclient.android;

import android.app.Activity;
import android.app.Application;
import android.content.ComponentName;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.pm.ApplicationInfo;
import android.content.res.AssetManager;
import android.content.res.Resources;
import android.os.Bundle;
import android.view.ContextThemeWrapper;

import java.io.File;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * En el proceso del juego (":minecraft") prepara el Minecraft instalado para
 * correr dentro de LeX:
 *  - su codigo (dex) se anade al cargador de clases de LeX, asi la actividad
 *    com.mojang.minecraftpe.MainActivity del manifiesto es la de Mojang;
 *  - sus librerias nativas se cargan en orden desde su carpeta;
 *  - la actividad usa sus recursos, sus assets y su tema (AppCompat).
 */
public class LexApplication extends Application {
    public static final String MINECRAFT = "com.mojang.minecraftpe";
    private static final String GAME_ACTIVITY = "com.mojang.minecraftpe.MainActivity";
    private static LexApplication instance;
    private Context minecraft;
    private int minecraftTheme;

    public static LexApplication get() {
        return instance;
    }

    static boolean isGameProcess() {
        String name = Application.getProcessName();
        return name != null && name.endsWith(":minecraft");
    }

    @Override
    protected void attachBaseContext(Context base) {
        super.attachBaseContext(base);
        instance = this;
        LexLog.init(base);
        final Thread.UncaughtExceptionHandler previous = Thread.getDefaultUncaughtExceptionHandler();
        Thread.setDefaultUncaughtExceptionHandler(new Thread.UncaughtExceptionHandler() {
            @Override
            public void uncaughtException(Thread thread, Throwable error) {
                LexLog.e("Cierre inesperado en " + thread.getName(), error);
                if (previous != null) previous.uncaughtException(thread, error);
            }
        });
        if (!isGameProcess()) return;
        LexLog.i("Proceso del juego: preparando Minecraft");
        try {
            // Solo recursos y assets: el codigo va a nuestro cargador (abajo).
            minecraft = base.createPackageContext(MINECRAFT, 0);
            ApplicationInfo info = minecraft.getApplicationInfo();
            describe(info);
            addCode(info);
            minecraftTheme = base.getPackageManager().getActivityInfo(new ComponentName(MINECRAFT, GAME_ACTIVITY), 0).getThemeResource();
            LexLog.i("Tema del juego: 0x" + Integer.toHexString(minecraftTheme));
        } catch (Throwable error) {
            LexLog.e("No se pudo preparar el Minecraft instalado", error);
        }
    }

    private static void describe(ApplicationInfo info) {
        StringBuilder text = new StringBuilder("Minecraft encontrado: ").append(info.sourceDir);
        if (info.splitSourceDirs != null) for (String split : info.splitSourceDirs) text.append(" + ").append(split);
        text.append(" | libs=").append(info.nativeLibraryDir);
        String[] libs = info.nativeLibraryDir != null ? new File(info.nativeLibraryDir).list() : null;
        text.append(" | ").append(libs == null ? "sin librerias" : Arrays.toString(libs));
        LexLog.i(text.toString());
    }

    /** Anade el dex de Minecraft y su carpeta de librerias al cargador de LeX. */
    private void addCode(ApplicationInfo info) throws Exception {
        ClassLoader loader = getClassLoader();
        Field pathListField = dalvik.system.BaseDexClassLoader.class.getDeclaredField("pathList");
        pathListField.setAccessible(true);
        Object pathList = pathListField.get(loader);
        Method addDexPath = pathList.getClass().getDeclaredMethod("addDexPath", String.class, File.class);
        addDexPath.setAccessible(true);
        List<String> apks = new ArrayList<>();
        apks.add(info.sourceDir);
        if (info.splitSourceDirs != null) apks.addAll(Arrays.asList(info.splitSourceDirs));
        for (String apk : apks) addDexPath.invoke(pathList, apk, null);
        if (info.nativeLibraryDir != null) {
            Method addNativePath = pathList.getClass().getDeclaredMethod("addNativePath", java.util.Collection.class);
            addNativePath.setAccessible(true);
            addNativePath.invoke(pathList, Arrays.asList(info.nativeLibraryDir));
            minecraftLibrariesFirst(pathList, info.nativeLibraryDir);
        }
        Class<?> game = loader.loadClass(GAME_ACTIVITY);
        LexLog.i("Codigo de Minecraft listo (" + apks.size() + " apk): " + game.getName());
    }

    /**
     * La carpeta de librerias de Minecraft se busca antes que /system/lib64:
     * si no, System.loadLibrary("c++_shared") del juego encuentra la del
     * sistema, que Android no deja usar a una app.
     */
    private static void minecraftLibrariesFirst(Object pathList, String dir) {
        try {
            Field field = pathList.getClass().getDeclaredField("nativeLibraryPathElements");
            field.setAccessible(true);
            Object[] elements = (Object[]) field.get(pathList);
            List<Object> first = new ArrayList<>();
            List<Object> rest = new ArrayList<>();
            for (Object e : elements) (String.valueOf(e).contains(dir) ? first : rest).add(e);
            first.addAll(rest);
            Object[] sorted = (Object[]) java.lang.reflect.Array.newInstance(elements.getClass().getComponentType(), elements.length);
            field.set(pathList, first.toArray(sorted));
            LexLog.i("Orden de busqueda de librerias: " + first);
        } catch (Throwable error) {
            LexLog.e("No se pudo ordenar la busqueda de librerias", error);
        }
    }

    /**
     * Librerias del juego. Cada una necesita otras (libminecraftpe.so pide
     * libPlayFabMultiplayer.so, que pide libpairipcore.so...): se cargan por
     * pasadas, reintentando las que fallan hasta que no queda ninguna o ya no
     * se avanza.
     */
    private void loadLibraries() {
        String dir = minecraft.getApplicationInfo().nativeLibraryDir;
        String[] names = dir != null ? new File(dir).list() : null;
        if (names == null) return;
        List<String> pending = new ArrayList<>(Arrays.asList("libc++_shared.so", "libfmod.so", "libpairipcore.so"));
        pending.retainAll(Arrays.asList(names));
        for (String name : names) if (name.endsWith(".so") && !pending.contains(name) && !name.equals("libminecraftpe.so")) pending.add(name);
        pending.add("libminecraftpe.so");
        Map<String, Throwable> errors = new LinkedHashMap<>();
        for (int pass = 1; !pending.isEmpty(); pass++) {
            List<String> failed = new ArrayList<>();
            errors.clear();
            for (String name : pending) {
                try {
                    System.load(new File(dir, name).getAbsolutePath());
                    LexLog.i("Cargada " + name + " (pasada " + pass + ")");
                } catch (Throwable error) {
                    failed.add(name);
                    errors.put(name, error);
                }
            }
            if (failed.size() == pending.size()) break;  // ya no se avanza
            pending = failed;
        }
        for (Map.Entry<String, Throwable> e : errors.entrySet()) LexLog.e("No se pudo cargar " + e.getKey(), e.getValue());
    }

    @Override
    public void onCreate() {
        super.onCreate();
        if (!isGameProcess() || minecraft == null) return;
        loadLibraries();
        registerActivityLifecycleCallbacks(new ActivityLifecycleCallbacks() {
            @Override
            public void onActivityPreCreated(Activity activity, Bundle state) {
                if (!activity.getClass().getName().startsWith("com.mojang.")) return;
                prepare(activity);
            }

            @Override
            public void onActivityPostCreated(Activity activity, Bundle state) {
                if (activity.getClass().getName().startsWith("com.mojang.")) LexInput.attach(activity);
            }

            @Override public void onActivityCreated(Activity activity, Bundle state) {}
            @Override public void onActivityStarted(Activity activity) {}
            @Override public void onActivityResumed(Activity activity) {}
            @Override public void onActivityPaused(Activity activity) {}
            @Override public void onActivityStopped(Activity activity) {}
            @Override public void onActivitySaveInstanceState(Activity activity, Bundle state) {}
            @Override public void onActivityDestroyed(Activity activity) {}
        });
    }

    /**
     * Antes de su onCreate, la actividad de Mojang pasa a ver los recursos y
     * assets del juego y toma su tema (GameActivity exige uno AppCompat).
     */
    private void prepare(Activity activity) {
        try {
            Field base = ContextWrapper.class.getDeclaredField("mBase");
            base.setAccessible(true);
            base.set(activity, new GameContext((Context) base.get(activity), minecraft));
            // Lo que ya se hubiera leido con los recursos de LeX, fuera.
            for (String name : new String[] {"mResources", "mTheme", "mInflater"}) {
                try {
                    Field f = ContextThemeWrapper.class.getDeclaredField(name);
                    f.setAccessible(true);
                    f.set(activity, null);
                } catch (Throwable ignored) {
                    // no existe en esta version de Android
                }
            }
            if (minecraftTheme != 0) activity.setTheme(minecraftTheme);
            LexLog.i("Contexto y tema del juego aplicados a " + activity.getClass().getName());
        } catch (Throwable error) {
            LexLog.e("No se pudo aplicar el contexto del juego", error);
        }
    }

    @Override
    public AssetManager getAssets() {
        return minecraft != null ? minecraft.getAssets() : super.getAssets();
    }

    @Override
    public Resources getResources() {
        return minecraft != null ? minecraft.getResources() : super.getResources();
    }

    /**
     * Contexto de la actividad del juego: recursos y assets de Minecraft, pero
     * el resto (paquete, carpetas, cargador de clases) de LeX.
     */
    static final class GameContext extends ContextWrapper {
        private final Context minecraft;

        GameContext(Context base, Context minecraft) {
            super(base);
            this.minecraft = minecraft;
        }

        @Override public AssetManager getAssets() { return minecraft.getAssets(); }
        @Override public Resources getResources() { return minecraft.getResources(); }
    }
}
