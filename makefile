G = gcc -g
O = -o
UILIBS = -lglut -lGLU -lGL -lm -lrt
names = parent pills liquid opengl
shared_args_file = arguments.txt

all: $(names) run_with_arguments

$(names): % : %.c
	$(G) $< $(O) $@ $(UILIBS)

run_with_arguments: parent
	./parent $$(cat $(shared_args_file))

clean:
	rm -f $(names)

