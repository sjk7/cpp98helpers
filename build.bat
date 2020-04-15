rem cmd.exe /c echo %MSBUILD_PATH%
cmd.exe /c "%MSBUILD_PATH%\MSBuild.exe" /p:Platform=x86 /p:Configuration=Release /p:RunCodeAnalysis=False SPSCAudio.sln 
