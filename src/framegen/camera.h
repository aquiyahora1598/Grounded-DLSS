#pragma once
#include "../grounded/contract.h"
#include <algorithm>
namespace grounded::framegen {
struct Camera {float position[3]{},right[3]{},up[3]{},forward[3]{};float nearPlane=0,fov=0,deltaMs=0,worldTime=0;};
using Matrix=std::array<double,16>;
inline Matrix Product(const Matrix& a,const Matrix& b){Matrix c{};for(unsigned r=0;r<4;++r)for(unsigned k=0;k<4;++k)for(unsigned s=0;s<4;++s)c[r*4+k]+=a[r*4+s]*b[s*4+k];return c;}
inline bool Inverse(Matrix a,Matrix& b){b={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    for(unsigned c=0;c<4;++c){unsigned pivot=c;for(unsigned r=c+1;r<4;++r)if(std::abs(a[r*4+c])>std::abs(a[pivot*4+c]))pivot=r;
        if(std::abs(a[pivot*4+c])<1e-12)return false;
        for(unsigned k=0;k<4;++k){std::swap(a[c*4+k],a[pivot*4+k]);std::swap(b[c*4+k],b[pivot*4+k]);}
        const double div=a[c*4+c];for(unsigned k=0;k<4;++k){a[c*4+k]/=div;b[c*4+k]/=div;}
        for(unsigned r=0;r<4;++r)if(r!=c){const double scale=a[r*4+c];for(unsigned k=0;k<4;++k){a[r*4+k]-=scale*a[c*4+k];b[r*4+k]-=scale*b[c*4+k];}}}
    return true;
}
inline bool DecodeCamera(const Constants& c,Camera& out){
    auto matrix=[&](unsigned reg){Matrix m{};std::copy_n(c.view.begin()+reg*4,16,m.begin());return m;};
    for(unsigned i=0;i<44*4;++i)if(!std::isfinite(c.view[i]))return false;
    const auto p=matrix(28),p0=matrix(32);Matrix shape{};shape[0]=p[0];shape[5]=p[5];shape[8]=p[8];shape[9]=p[9];shape[11]=1;shape[14]=p[14];
    if(p[0]<=0 || p[5]<=0 || p[14]<=0 || std::abs(p[8])>.02 || std::abs(p[9])>.02)return false;
    for(unsigned i=0;i<16;++i)if(std::abs(p[i]-shape[i])>1e-5 || std::abs(p0[i]-((i==8 || i==9)?0:shape[i]))>1e-5)return false;
    auto identity=[](const Matrix& m){for(unsigned i=0;i<16;++i)if(std::abs(m[i]-(i%5==0?1.0:0.0))>1e-4)return false;return true;};
    if(!identity(Product(p,matrix(36))) || !identity(Product(matrix(0),matrix(40))))return false;
    Matrix invP{},camera{};if(!Inverse(p,invP))return false;
    const auto translated=Product(matrix(0),invP),view=Product(matrix(4),invP),nativeView=matrix(12);
    for(unsigned r=0;r<4;++r)for(unsigned k=0;k<4;++k){const auto i=r*4+k;
        if(std::abs(translated[i]-nativeView[i])>1e-4 || (r<3 && k<3 && std::abs(view[i]-nativeView[i])>1e-4))return false;}
    if(std::abs(view[3])>1e-4 || std::abs(view[7])>1e-4 || std::abs(view[11])>1e-4 || std::abs(view[15]-1)>1e-4 || !Inverse(view,camera))return false;
    for(unsigned r=0;r<3;++r)for(unsigned k=0;k<3;++k){double dot=0;for(unsigned i=0;i<3;++i)dot+=camera[r*4+i]*camera[k*4+i];if(std::abs(dot-(r==k?1:0))>1e-4)return false;}
    for(unsigned i=0;i<3;++i){out.position[i]=float(camera[12+i]);out.right[i]=float(camera[i]);out.up[i]=float(camera[4+i]);out.forward[i]=float(camera[8+i]);}
    out.nearPlane=float(p[14]);out.fov=float(2*std::atan(1/p0[5]));out.deltaMs=c.view[143*4]*1000;out.worldTime=c.view[142*4+2];
    return std::isfinite(out.deltaMs) && out.deltaMs>=0 && out.deltaMs<1000 && std::isfinite(out.worldTime);
}
}
