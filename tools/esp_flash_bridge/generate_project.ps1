$source = Join-Path $PSScriptRoot '..\..\smart-pill-box.uvprojx'
$destination = Join-Path $PSScriptRoot 'ESP_Flash_Bridge.uvprojx'
$project = [IO.File]::ReadAllText($source)
$groups = [regex]::Matches($project, '(?s)<Group>.*?</Group>')

# Keil rejects XmlDocument's normalized serialization. Keep its project formatting.
for ($groupIndex = $groups.Count - 1; $groupIndex -ge 0; $groupIndex--) {
    $group = $groups[$groupIndex]
    $name = [regex]::Match($group.Value, '<GroupName>([^<]+)</GroupName>').Groups[1].Value
    if ($name -notin @('start', 'user', 'library')) {
        $project = $project.Remove($group.Index, $group.Length)
        continue
    }

    $block = $group.Value
    $files = [regex]::Matches($block, '(?s)<File>.*?</File>')
    for ($fileIndex = $files.Count - 1; $fileIndex -ge 0; $fileIndex--) {
        $file = $files[$fileIndex]
        $fileName = [regex]::Match($file.Value, '<FileName>([^<]+)</FileName>').Groups[1].Value
        $keep = ($name -eq 'start' -and $fileName -in @('core_cm3.c', 'startup_stm32f10x_hd.s', 'system_stm32f10x.c')) -or
                ($name -eq 'library' -and $fileName -in @('stm32f10x_gpio.c', 'stm32f10x_rcc.c', 'stm32f10x_usart.c')) -or
                ($name -eq 'user' -and $fileName -eq 'main.c')
        if (-not $keep) { $block = $block.Remove($file.Index, $file.Length) }
    }

    if ($name -eq 'user') {
        $block = $block.Replace('<GroupName>user</GroupName>', '<GroupName>bridge</GroupName>')
        $block = $block.Replace('<FileName>main.c</FileName>', '<FileName>bridge_main.c</FileName>')
        $block = $block.Replace('<FilePath>.\user\main.c</FilePath>', '<FilePath>.\bridge_main.c</FilePath>')
    } else {
        $block = $block.Replace('<FilePath>.\' + $name + '\', '<FilePath>..\..\' + $name + '\')
    }
    $project = $project.Remove($group.Index, $group.Length).Insert($group.Index, $block)
}

$project = $project.Replace('<TargetName>Target 1</TargetName>', '<TargetName>ESP Flash Bridge</TargetName>')
$project = $project.Replace('<OutputDirectory>.\Objects\</OutputDirectory>', '<OutputDirectory>.\BridgeObjects\</OutputDirectory>')
$project = $project.Replace('<ListingPath>.\Listings\</ListingPath>', '<ListingPath>.\BridgeListings\</ListingPath>')
$project = $project.Replace('<OutputName>Project</OutputName>', '<OutputName>ESP_Flash_Bridge</OutputName>')
$project = $project.Replace('<IncludePath>.\library;.\start;.\user;.\user\app;.\user\bsp;.\system</IncludePath>',
                            '<IncludePath>..\..\library;..\..\start;..\..\user</IncludePath>')
$project = [regex]::Replace($project, '(?m)^[ \t]+(?=\r?$)', '')

[IO.File]::WriteAllText($destination, $project, [Text.UTF8Encoding]::new($false))
Write-Output $destination
