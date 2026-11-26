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
                LexLog.i("Actividad del juego creada: " + className);
                return activity;
            } catch (ReflectiveOperationException error) {
                LexLog.e("No se pudo crear " + className, error);
                throw new InstantiationException(error.toString());
            }
        }
        return super.instantiateActivity(loader, className, intent);
    }
}
