set PATH=$MSBUILD_PATH:$PATH
cmd.exe /c echo %PATH%
cmd.exe /C MSBuild.exe /p:Platform=x86 /p:Configuration=Release /p:RunCodeAnalysis=False SPSCAudio.sln   