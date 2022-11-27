NAME = qsim
OUT = .
CC = cc
CFLAGS = -lm -lpthread -O2
FILES = main.c qsim.c

clean:
	rm -f $(OUT)/$(NAME)

$(NAME): clean
	$(CC) $(CFLAGS) -o $(OUT)/$(NAME) $(FILES)
