CFLAGS = -std=c++17 -g -I../third_party -I../src
LDFLAGS = -lSDL2 -lvulkan -llz4 -ldl

deferred: imgui.o tiny_obj_loader.o boot.o asset_packer.o ../src/*.cpp
	g++ $(CFLAGS) -o app VkBootstrap.o imgui*.o tiny_obj_loader.o asset_packer.o ../src/*.cpp ../src/deferred/*.cpp $(LDFLAGS)
	./build_shaders

ao: imgui.o tiny_obj_loader.o boot.o asset_packer.o ../src/*.cpp
	g++ $(CFLAGS) -o app VkBootstrap.o imgui*.o tiny_obj_loader.o asset_packer.o ../src/*.cpp ../src/ao/*.cpp $(LDFLAGS)
	./build_shaders

imgui.o: ../third_party/imgui/*.cpp
	g++ -c -I../third_party/imgui -std=c++17 ../third_party/imgui/*.cpp $(LDFLAGS)

tiny_obj_loader.o:
	g++ -c -I../third_party/tinyobjloader -std=c++17 ../third_party/tinyobjloader/*.cc $(LDFLAGS)

boot.o: ../third_party/vkbootstrap/*.cpp
	g++ -c -I../third_party/vkbootstrap -std=c++17 ../third_party/vkbootstrap/*.cpp $(LDFLAGS)

asset_packer.o:
	g++ -c -g -I../third_party -I../src/asset_packer -std=c++17 ../src/asset_packer/asset_packer.cpp $(LDFLAGS)

clean:
	rm -f app

asset: asset_packer.o tiny_obj_loader.o
	g++ $(CFLAGS) -o asset_packer tiny_obj_loader.o ../src/asset_packer/*.cpp $(LDFLAGS)

