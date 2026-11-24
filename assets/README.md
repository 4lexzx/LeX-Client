# Assets

## Iconos (Fase 2)

`icons/` contiene 28 iconos lineales **originales**, generados con
`scripts/make-icons.mjs` (sin copiar recursos de Mojang ni de otros clientes):

- `<nombre>_32.png` y `<nombre>_64.png` — fuente editable / vista previa.
- `<nombre>_32.rgba` — pixeles RGBA crudos que se empaquetan como RCDATA en
  `LeXClient.dll` mediante `platform/windows/resources/icons.rc.in`.
- `preview.png` — contact sheet con los 28 iconos.

Regenerar tras cambiar el script:

```powershell
node scripts/make-icons.mjs
```

No hace falta Node para compilar: los archivos generados estan commiteados.

## Pendiente

- `fonts/` — tipografia propia (Fase 5).
- `cosmetics/` — capas/gorros (Fase 6).
