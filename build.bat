setx path /M "$MSBUILD_PATH;%path%"
cmd.exe /c "%MSBUILD_PATH%\MSBuild.exe" /p:Platform=x86 /p:Configuration=Release /p:RunCodeAnalysis=False SPSCAudio.sln

