package com.lexclient.android;

import android.app.Activity;
import android.app.Application;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.res.AssetManager;
import android.content.res.Resources;
import android.os.Bundle;

import java.lang.reflect.Field;

/**
 * En el proceso del juego (":minecraft") hace de puente con el Minecraft
 * instalado: su cargador de clases, sus recursos y sus assets.
 */
public class LexApplication extends Application {
    public static final String MINECRAFT = "com.mojang.minecraftpe";
    private static LexApplication instance;
    private Context minecraft;

    public static LexApplication get() {
        return instance;
    }

    static boolean isGameProcess() {
        String name = Application.getProcessName();
        return name != null && name.endsWith(":minecraft");
    }

    /** Cargador de clases del Minecraft instalado, o null si no esta. */
    public static ClassLoader minecraftLoader() {
        return instance != null && instance.minecraft != null ? instance.minecraft.getClassLoader() : null;
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
            minecraft = base.createPackageContext(MINECRAFT, Context.CONTEXT_INCLUDE_CODE | Context.CONTEXT_IGNORE_SECURITY);
            LexLog.i("Minecraft encontrado: " + minecraft.getApplicationInfo().sourceDir + " libs=" + minecraft.getApplicationInfo().nativeLibraryDir);
        } catch (Exception error) {
            LexLog.e("No se pudo abrir el Minecraft instalado", error);
        }
    }

    @Override
    public void onCreate() {
        super.onCreate();
        if (!isGameProcess() || minecraft == null) return;
        // Antes de que la actividad de Mojang arranque, su contexto base pasa a
        // dar los recursos y assets del juego (los nuestros no los tiene).
        registerActivityLifecycleCallbacks(new ActivityLifecycleCallbacks() {
            @Override
            public void onActivityPreCreated(Activity activity, Bundle state) {
                if (!activity.getClass().getName().startsWith("com.mojang.")) return;
                try {
                    Field base = ContextWrapper.class.getDeclaredField("mBase");
                    base.setAccessible(true);
                    Context original = (Context) base.get(activity);
                    base.set(activity, new GameContext(original, minecraft));
                    LexLog.i("Contexto del juego aplicado a " + activity.getClass().getName());
                } catch (Throwable error) {
                    LexLog.e("No se pudo aplicar el contexto del juego", error);
                }
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
     * el resto (paquete, carpetas, cargador de nuestra libreria puente) de LeX.
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
