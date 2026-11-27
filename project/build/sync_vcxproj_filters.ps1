# sync_vcxproj_filters.ps1
# .vcxproj に登録されたファイルの置き場所（フォルダ）から、.vcxproj.filters を作り直す。
# ソリューションエクスプローラーの並びが、物理フォルダと同じになる。
#   - フィルターは、ファイルのあるフォルダと、その上のフォルダ全部（例：DirectX\engine\3d\Model.cpp → DirectX・DirectX\engine・DirectX\engine\3d）。
#   - フィルターの UniqueIdentifier は、プロジェクト名とフォルダのパスから決まる値にする（何回動かしても同じ）。
#   - .vcxproj は読むだけで、書き換えない。
# 使い方：powershell -NoProfile -ExecutionPolicy Bypass -File build\sync_vcxproj_filters.ps1 [-Projects 名前.vcxproj,...] [-Check]
#   -Check を付けると書き出さず、今の .filters と違えば 1 を返す。
param(
    [string[]]$Projects = @("MyEngine.vcxproj", "MyEngineCore.vcxproj"),
    [switch]$Check
)

$ErrorActionPreference = "Stop"
# -File で動かすと「a,b」は1つの文字列で届くので、ここで分ける。
$Projects = @($Projects | ForEach-Object { $_ -split "," } | Where-Object { $_ -ne "" })
$projectRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
$namespace = "http://schemas.microsoft.com/developer/msbuild/2003"
# ファイルではない項目（構成・参照）は、フィルターに入れない。
$skipped = @("ProjectConfiguration", "ProjectReference")
$ordinal = [System.StringComparer]::OrdinalIgnoreCase

function Get-FilterGuid([string]$project, [string]$filter) {
    $md5 = [System.Security.Cryptography.MD5]::Create()
    try {
        $bytes = $md5.ComputeHash([System.Text.Encoding]::UTF8.GetBytes(($project + "|" + $filter).ToLowerInvariant()))
    } finally {
        $md5.Dispose()
    }
    return "{" + ([guid]::new($bytes)).ToString() + "}"
}

function Escape-Xml([string]$text) {
    return [System.Security.SecurityElement]::Escape($text)
}

$changed = $false
foreach ($name in $Projects) {
    $projectPath = Join-Path $projectRoot $name
    if (-not (Test-Path -LiteralPath $projectPath)) {
        throw "プロジェクトが見つからない: $projectPath"
    }
    $xml = New-Object System.Xml.XmlDocument
    $xml.Load($projectPath)
    $manager = New-Object System.Xml.XmlNamespaceManager($xml.NameTable)
    $manager.AddNamespace("m", $namespace)

    # 種類（ClCompile・ClInclude など）ごとの項目と、要るフィルター。
    $itemsByType = @{}
    $typeOrder = New-Object System.Collections.Generic.List[string]
    $filters = New-Object System.Collections.Generic.SortedSet[string]($ordinal)
    foreach ($node in $xml.SelectNodes("/m:Project/m:ItemGroup/*", $manager)) {
        $type = $node.LocalName
        $include = $node.GetAttribute("Include")
        if ($skipped -contains $type -or [string]::IsNullOrEmpty($include)) {
            continue
        }
        if ($include.StartsWith("..") -or [System.IO.Path]::IsPathRooted($include)) {
            throw "プロジェクトのフォルダの外のファイルは扱わない: $include"
        }
        if (-not $itemsByType.ContainsKey($type)) {
            $itemsByType[$type] = New-Object System.Collections.Generic.List[string]
            $typeOrder.Add($type)
        }
        $itemsByType[$type].Add($include)
        $folder = [System.IO.Path]::GetDirectoryName($include)
        while (-not [string]::IsNullOrEmpty($folder)) {
            [void]$filters.Add($folder)
            $folder = [System.IO.Path]::GetDirectoryName($folder)
        }
    }

    $builder = New-Object System.Text.StringBuilder
    $newline = "`r`n"
    [void]$builder.Append('<?xml version="1.0" encoding="utf-8"?>' + $newline)
    [void]$builder.Append('<Project ToolsVersion="4.0" xmlns="' + $namespace + '">' + $newline)
    [void]$builder.Append('  <ItemGroup>' + $newline)
    foreach ($filter in $filters) {
        [void]$builder.Append('    <Filter Include="' + (Escape-Xml $filter) + '">' + $newline)
        [void]$builder.Append('      <UniqueIdentifier>' + (Get-FilterGuid $name $filter) + '</UniqueIdentifier>' + $newline)
        [void]$builder.Append('    </Filter>' + $newline)
    }
    [void]$builder.Append('  </ItemGroup>' + $newline)
    foreach ($type in $typeOrder) {
        $items = $itemsByType[$type].ToArray()
        [System.Array]::Sort($items, $ordinal)
        [void]$builder.Append('  <ItemGroup>' + $newline)
        foreach ($include in $items) {
            $folder = [System.IO.Path]::GetDirectoryName($include)
            if ([string]::IsNullOrEmpty($folder)) {
                # プロジェクトのフォルダの直下（main.cpp など）は、フィルターに入れない。
                [void]$builder.Append('    <' + $type + ' Include="' + (Escape-Xml $include) + '" />' + $newline)
            } else {
                [void]$builder.Append('    <' + $type + ' Include="' + (Escape-Xml $include) + '">' + $newline)
                [void]$builder.Append('      <Filter>' + (Escape-Xml $folder) + '</Filter>' + $newline)
                [void]$builder.Append('    </' + $type + '>' + $newline)
            }
        }
        [void]$builder.Append('  </ItemGroup>' + $newline)
    }
    [void]$builder.Append('</Project>')
    $text = $builder.ToString()

    $filtersPath = $projectPath + ".filters"
    $current = if (Test-Path -LiteralPath $filtersPath) { [System.IO.File]::ReadAllText($filtersPath) } else { "" }
    if ($current -ceq $text) {
        Write-Host "変わらない: $name.filters（フィルター $($filters.Count)）"
        continue
    }
    $changed = $true
    if ($Check) {
        Write-Host "違う: $name.filters"
        continue
    }
    [System.IO.File]::WriteAllText($filtersPath, $text, (New-Object System.Text.UTF8Encoding($false)))
    $count = 0
    foreach ($type in $typeOrder) { $count += $itemsByType[$type].Count }
    Write-Host "書き出した: $name.filters（フィルター $($filters.Count)・ファイル $count）"
}
if ($Check -and $changed) {
    exit 1
}
