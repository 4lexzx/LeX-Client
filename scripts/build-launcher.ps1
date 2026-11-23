# LEX CLIENT — Build del launcher (renderer + empaquetado NSIS).
Write-Host "=== build-launcher ==="
Push-Location $PSScriptRoot/../apps/launcher
try {
    Write-Host "-> npm run build (vite build) + electron-builder --config electron-builder.yml"
    npm run build
    npm run package
} finally {
    Pop-Location
}
