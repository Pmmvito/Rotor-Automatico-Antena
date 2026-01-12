# Script para remover BOM de todos arquivos .cpp e .h
$files = Get-ChildItem -Path . -Include *.cpp,*.h -Recurse

foreach ($file in $files) {
    $content = [System.IO.File]::ReadAllBytes($file.FullName)
    
    # Verificar se tem BOM (EF BB BF)
    if ($content.Length -ge 3 -and $content[0] -eq 0xEF -and $content[1] -eq 0xBB -and $content[2] -eq 0xBF) {
        # Remover BOM
        $newContent = $content[3..($content.Length-1)]
        [System.IO.File]::WriteAllBytes($file.FullName, $newContent)
        Write-Host "BOM removido de: $($file.Name)" -ForegroundColor Green
    } else {
        Write-Host "Sem BOM: $($file.Name)" -ForegroundColor Gray
    }
}

Write-Host "`nConcluido!" -ForegroundColor Cyan
