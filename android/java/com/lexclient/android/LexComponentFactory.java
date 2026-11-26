package com.lexclient.android;

import android.app.Activity;
import android.app.AppComponentFactory;
import android.content.Intent;

/**
 * Las actividades com.mojang.* se crean con las clases del Minecraft instalado
 * (no estan en este APK): asi la actividad del juego es la original de Mojang.
 */
public class LexComponentFactory extends AppComponentFactory {
    @Override
    public Activity instantiateActivity(ClassLoader loader, String className, Intent intent)
            throws InstantiationException, IllegalAccessException, ClassNotFoundException {
        if (className.startsWith("com.mojang.")) {
            ClassLoader minecraft = LexApplication.minecraftLoader();
            if (minecraft == null) throw new ClassNotFoundException("Minecraft no esta instalado");
            Thread.currentThread().setContextClassLoader(minecraft);
            try {
                Activity activity = (Activity) Class.forName(className, true, minecraft).getDeclaredConstructor().newInstance();
                LexLog.i("Actividad del juego creada: " + className + " (libreria del juego ya cargada: " + gameLibraryLoaded() + ")");
                return activity;
            } catch (ReflectiveOperationException error) {
                LexLog.e("No se pudo crear " + className, error);
                throw new InstantiationException(error.toString());
            }
        }
        return super.instantiateActivity(loader, className, intent);
    }

    /** Si las clases de Mojang ya cargaron su libminecraftpe.so (se ve en /proc/self/maps). */
    private static boolean gameLibraryLoaded() {
        try (java.io.BufferedReader maps = new java.io.BufferedReader(new java.io.FileReader("/proc/self/maps"))) {
            for (String line; (line = maps.readLine()) != null; ) {
                if (line.contains("com.mojang.minecraftpe") && (line.contains("libminecraftpe.so") || line.contains(".apk"))) {
                    if (line.contains("r-xp")) return true;
                }
            }
        } catch (Exception ignored) {
            // sin acceso: se sabra por el registro nativo
        }
        return false;
    }
}
