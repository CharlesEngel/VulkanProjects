CFLAGS = -std=c++17 -g -I../third_party
LDFLAGS = -lSDL2 -lvulkan

app: imgui.o tob.o boot.o *.cpp
	g++ $(CFLAGS) -o app *.o *.cpp $(LDFLAGS)

imgui.o: ../third_party/imgui/*.cpp
	g++ -c -I../third_party/imgui -std=c++17 ../third_party/imgui/*.cpp $(LDFLAGS)

tob.o: ../third_party/tiny_obj_loader/*.cc
	g++ -c -I../third_party/tinyobjloader -std=c++17 ../third_party/tinyobjloader/*.cc $(LDFLAGS)

boot.o: ../third_party/vkbootstrap/*.cpp
	g++ -c -I../third_party/vkbootstrap -std=c++17 ../third_party/vkbootstrap/*.cpp $(LDFLAGS)


clean:
	rm -f app
