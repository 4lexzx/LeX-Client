# Modelo de interfaz (core/gui)

Fase 1: sin archivos de codigo. El modelo de ClickGUI, ventanas y editor de HUD
se anade en Fase 2 y 3.

Regla arquitectonica: esta carpeta contiene SOLO logica (estado de ventanas,
filtros, favoritos, layout). Nunca incluye ImGui, DirectX ni Win32. El render
lo hace la capa `platform/windows` usando este modelo.
