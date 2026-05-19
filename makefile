.RECIPEPREFIX = @

all: 
@ gcc main.c bridge.c show.c -o bridge -pthread

clean:
@ rm -f bridge