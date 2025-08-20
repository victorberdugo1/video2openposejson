NAME = raypup

# Directorios
INC_DIR = inc
SRC_DIR = src
OBJ_DIR = obj

# Archivos fuente
SRC = main.c bonetile.c bones3d.c

# Rutas completas de los fuentes
SRCS = $(addprefix $(SRC_DIR)/, $(SRC))

# Objetos
OBJ = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

# Raylib
RAYLIB_DIR = raylib
RAYLIB_LIB = libraylib.a

# Compilador y flags
CC = cc
CFLAGS = -Wall -Wextra -Werror -I$(INC_DIR) -I$(RAYLIB_DIR)/src -g -fsanitize=address -fsanitize=leak
LDFLAGS = -L. -lraylib -lm -ldl -lpthread -lGL -lX11

# Colores
GREEN = \033[0;32m
RED = \033[0;91m
RESET = \033[0m

all: $(OBJ_DIR) $(NAME)

# Asegurarnos de que raylib esté compilado
$(RAYLIB_LIB):
	@if [ ! -d "$(RAYLIB_DIR)" ]; then \
		echo "$(RESET)Cloning raylib..."; \
		git clone https://github.com/raysan5/raylib.git $(RAYLIB_DIR); \
	fi
	@echo "$(RESET)Building raylib..."; \
	$(MAKE) -C $(RAYLIB_DIR)/src PLATFORM=PLATFORM_DESKTOP RAYLIB_LIBTYPE=STATIC; \
	cp $(RAYLIB_DIR)/src/$(RAYLIB_LIB) .

$(NAME): $(RAYLIB_LIB) $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDFLAGS)
	@echo "$(GREEN)Executable $(NAME) successfully created.$(RESET)"

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@
	@echo "$(GREEN)Compiled: $<$(RESET)"

$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

clean:
	rm -rf $(OBJ_DIR)
	@echo "$(RED)Object files deleted.$(RESET)"

fclean: clean
	rm -f $(NAME)
	rm -f $(RAYLIB_LIB)
	@echo "$(RED)Executable and libraries deleted.$(RESET)"

re: fclean all

.PHONY: all clean fclean re

