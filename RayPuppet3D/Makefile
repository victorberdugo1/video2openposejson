NAME = raypup

# Directorios
INC_DIR = inc
SRC_DIR = src
OBJ_DIR = obj

# Archivos fuente
SRC = main.c bones3d.c

# Rutas completas de los fuentes
SRCS = $(addprefix $(SRC_DIR)/, $(SRC))

# Objetos
OBJ = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

# Raylib
RAYLIB_DIR = raylib
RAYLIB_LIB = libraylib.a

# =============================
# Compilador según sistema
# =============================
ifeq ($(OS),Windows_NT)
	# Windows
	CC = gcc
	CFLAGS = -Wall -Wextra -Werror -I$(INC_DIR) -I$(RAYLIB_DIR)/src -g -MMD -MP
	LDFLAGS = -L. -lraylib -lopengl32 -lgdi32 -lwinmm -lole32 -lws2_32 -lm
else
	# Linux / POSIX
	CC = cc
	CFLAGS = -Wall -Wextra -Werror -I$(INC_DIR) -I$(RAYLIB_DIR)/src -g -fsanitize=address -fsanitize=leak -MMD -MP
	LDFLAGS = -L. -lraylib -lm -ldl -lpthread -lGL -lX11
endif

# Colores
GREEN = \033[0;32m
RED = \033[0;91m
RESET = \033[0m

# Target principal
all: $(OBJ_DIR) $(RAYLIB_LIB) $(NAME)

# ============================
# Compilar Raylib si no existe
# ============================
$(RAYLIB_LIB):
	@echo "$(RESET)Checking for raylib..."
	@if [ ! -d "$(RAYLIB_DIR)" ]; then \
		echo "Cloning raylib..."; \
		git clone https://github.com/raysan5/raylib.git "$(RAYLIB_DIR)"; \
	fi
	@echo "Building raylib..."
	@$(MAKE) -C "$(RAYLIB_DIR)/src" PLATFORM=PLATFORM_DESKTOP RAYLIB_LIBTYPE=STATIC
	@cp -f "$(RAYLIB_DIR)/src/$(RAYLIB_LIB)" .

# ============================
# Compilar programa principal
# ============================
$(NAME): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDFLAGS)
	@echo "$(GREEN)Executable $(NAME) created.$(RESET)"

# ============================
# Compilar cada archivo .c
# ============================
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p "$(@D)"
	$(CC) $(CFLAGS) -c $< -o $@
	@echo "$(GREEN)Compiled: $<$(RESET)"

# ============================
# Crear carpeta obj si falta
# ============================
$(OBJ_DIR):
	@mkdir -p "$(OBJ_DIR)"

# ============================
# Limpieza
# ============================
clean:
	@rm -rf "$(OBJ_DIR)"
	@echo "$(RED)Object files removed.$(RESET)"

fclean: clean
	@rm -f "$(NAME)" "$(RAYLIB_LIB)"
	@echo "$(RED)Executable and libraries removed.$(RESET)"

re: fclean all

.PHONY: all clean fclean re

# Incluir dependencias generadas automáticamente (.d)
-include $(OBJ:.o=.d)
