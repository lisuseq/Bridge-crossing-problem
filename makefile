.RECIPEPREFIX = @

all: 
@ gcc bridge.c -o bridge -pthread

clean:
@ rm -f bridge