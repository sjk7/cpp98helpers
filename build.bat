setx path /M "%path%;$MSBUILD_PATH"
cmd.exe /c echo %PATH%
cmd.exe /C MSBuild.exe /p:Platform=x86 /p:Configuration=Debug /p:RunCodeAnalysis=False SPSCAudio.sln 
