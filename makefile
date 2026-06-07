.RECIPEPREFIX = @

all:
@ mkdir -p bin 
@ gcc bridge.c -o bin/bridge.exe -pthread

clean:
@ rm -f bin/*