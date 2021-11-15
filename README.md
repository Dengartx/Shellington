# Shellington
A linux shell project of team "Papatya"
To run you will just need to run the out file after compilation
Can run any command binary in $PATH environment variable without using execvp by getenv to create the path for execv
Reccommended to run the stock commands installed on the pc of your own but can run newly installed binaries but proceed with caution
Custom commands:
  short set $(Alias): will set the current pwd in a file where the .out file is
  short jump $(Alias): will cd to the directory corresponding to the alias
  bookmark:
    ls -la(or any command): will set any command as a bookmarked command at index i (usage: bookmark ls -la, no  " needed)
    -i $(i): will run the command on that index
    -d $(i): will delete command on that index
    -l: will list all the commands set on bookmark
  
