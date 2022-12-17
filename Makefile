
CFLAGS := -O3 -Wall -Wextra -fPIC $(shell sdl2-config --cflags)
LIBS := $(shell sdl2-config --libs) -lSDL2_gfx -lSDL2_image
OBJECTS := e6809.o e8910.o osint.o vecx.o
TARGET := libvecx.so
CLEANFILES := $(TARGET) $(OBJECTS)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -shared -o $@ $^ $(LIBS)

clean:
	$(RM) $(CLEANFILES)

