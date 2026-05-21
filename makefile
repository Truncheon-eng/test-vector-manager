SRC := writemem.c
EXE := writemem.out
CCX := gcc
FLAGS := -g


.PHONY: all clean

all: $(EXE)

$(EXE): $(SRC)
	$(CXX) $(FLAGS) $< -o $@


clean:
	if [ -f "$(EXE)" ]; then \
		rm -f "$(EXE)"; \
	fi 