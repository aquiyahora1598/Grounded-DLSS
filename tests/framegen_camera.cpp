#include "../src/framegen/camera.h"
#include <fstream>
#include <iostream>
#include <limits>
int main(int argc,char** argv){
    if(argc<2)return 2;
    for(int i=1;i<argc;++i){grounded::Constants c;std::ifstream f(argv[i],std::ios::binary);f.read(reinterpret_cast<char*>(c.view.data()),sizeof(c.view));
        grounded::framegen::Camera camera;if(!f || !grounded::framegen::DecodeCamera(c,camera))return 3;
        std::cout<<camera.position[0]<<' '<<camera.position[1]<<' '<<camera.position[2]<<' '<<camera.nearPlane<<' '<<camera.fov<<'\n';
        c.view[0]=std::numeric_limits<float>::quiet_NaN();if(grounded::framegen::DecodeCamera(c,camera))return 4;
    }
    return 0;
}
