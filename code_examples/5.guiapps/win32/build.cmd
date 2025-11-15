mkdir _build
copy gui_hello.exe.manifest _build
cd _build
gcc ../main.c -mwindows -o gui_hello.exe