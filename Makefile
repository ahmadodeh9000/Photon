CC = gcc
CFLAGS = `pkg-config --cflags gtk+-3.0 gtksourceview-4`
LIBS = `pkg-config --libs gtk+-3.0 gtksourceview-4`
TARGET = Photon
SRC = main.c

.PHONY: all clean install-lang

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(SRC) -o $(TARGET) $(CFLAGS) $(LIBS)

# Installs the Lunar language spec and VS Code-style scheme into the
# user's GtkSourceView 4 search paths so highlighting works out of the box.
install-lang:
	mkdir -p ~/.local/share/gtksourceview-4/language-specs
	mkdir -p ~/.local/share/gtksourceview-4/styles
	cp lang/lunar.lang ~/.local/share/gtksourceview-4/language-specs/
	cp styles/vscode-dark.xml ~/.local/share/gtksourceview-4/styles/

clean:
	rm -f $(TARGET)