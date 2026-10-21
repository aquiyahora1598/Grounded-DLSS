#include "../src/grounded/focus_time.h"
#include <cstdio>
#include <stdexcept>
#include <initializer_list>
int main()try{
 using grounded::focus_time::Coefficient;
 for(double hz:{15.,30.,45.,60.,72.,120.}) {
  double value=0;const int steps=int(hz*2);
  for(int n=0;n<steps;++n)value+=(1-value)*Coefficient(1/hz);
  if(std::abs(value-(1-std::exp(-8.)))>1e-10)throw std::runtime_error("Focus differs at equal elapsed time");
 }
 for(double dt:{0.,-1.,1.,double(NAN),double(INFINITY)})if(Coefficient(dt)!=.1)throw std::runtime_error("Invalid clock interval not guarded");
 double value=0;for(double dt:{.004,.08,.016,.2,.05,.15})value+=(1-value)*Coefficient(dt);
 if(std::abs(value-(1-std::exp(-.5/.25)))>1e-10)throw std::runtime_error("Variable frame time changed response");
 printf("PASS: equal focus response at 15..120 updates/s, irregular intervals, first-call/stall/invalid-clock guard.\n");return 0;
}catch(const std::exception& e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
