package com.lexclient.android;

import android.app.Activity;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.Window;

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;

/**
 * Toques, raton y teclado del juego pasan primero por LeX (el panel nativo).
 * Minecraft usa GameActivity, que recibe la entrada en Java; por eso se
 * intercepta aqui, en la ventana, antes que el juego. Si LeX la usa (panel
 * abierto, boton LeX, tecla X), el juego no la ve.
 */
final class LexInput {
    private LexInput() {}

    // Los registra la parte nativa al arrancar el juego (RegisterNatives).
    static native boolean pointer(int action, float x, float y, boolean mouse, float scroll);

    static native boolean key(int code, boolean down, int meta, int repeat);

    private static boolean linked = true;

    static void attach(Activity activity) {
        final Window window = activity.getWindow();
        final Window.Callback original = window.getCallback();
        if (original == null) return;
        Object proxy = Proxy.newProxyInstance(Window.Callback.class.getClassLoader(), new Class<?>[] {Window.Callback.class}, new InvocationHandler() {
            @Override
            public Object invoke(Object self, Method method, Object[] args) throws Throwable {
                String name = method.getName();
                if (linked && args != null && args.length == 1) {
                    try {
                        if ((name.equals("dispatchTouchEvent") || name.equals("dispatchGenericMotionEvent")) && motion((MotionEvent) args[0])) return true;
                        if (name.equals("dispatchKeyEvent")) {
                            KeyEvent k = (KeyEvent) args[0];
                            if (k.getAction() != KeyEvent.ACTION_MULTIPLE && key(k.getKeyCode(), k.getAction() == KeyEvent.ACTION_DOWN, k.getMetaState(), k.getRepeatCount())) return true;
                        }
                    } catch (UnsatisfiedLinkError notYet) {
                        // la parte nativa aun no esta: todo al juego
                    } catch (Throwable error) {
                        linked = false;
                        LexLog.e("Entrada de LeX desactivada", error);
                    }
                }
                try {
                    return method.invoke(original, args);
                } catch (InvocationTargetException error) {
                    throw error.getCause();
                }
            }
        });
        window.setCallback((Window.Callback) proxy);
        LexLog.i("Entrada de LeX enganchada a la ventana del juego");
    }

    private static boolean motion(MotionEvent e) {
        boolean mouse = e.isFromSource(InputDevice.SOURCE_MOUSE);
        float scroll = e.getActionMasked() == MotionEvent.ACTION_SCROLL ? e.getAxisValue(MotionEvent.AXIS_VSCROLL) : 0;
        return pointer(e.getActionMasked(), e.getX(), e.getY(), mouse, scroll);
    }
}
