#include <iostream>
#include <iomanip>
#include <random>
#include <chrono>
#include <mutex>
#include "HoareMonitor.hpp"

using namespace HM;

//Variables globales------------------------------------------------------------
constexpr int
  num_fumadores = 3;           // número de fumadores
mutex
  mtx ;                        // mutex de escritura en pantalla

//Generador de números aleatorios-----------------------------------------------
template< int min, int max > int aleatorio(){
  static default_random_engine generador( (random_device())() );
  static uniform_int_distribution<int> distribucion_uniforme( min, max ) ;
  return distribucion_uniforme( generador );
}

//Produce un ingrediente entre 0 y (num_fumadores-1)----------------------------
int producirIngrediente(){
  int igr = aleatorio<0,num_fumadores-1>();
  return igr;
}

void fumar(int num_fumador){
  // calcular milisegundos aleatorios de duración de la acción de fumar)
  chrono::milliseconds duracion_fumar( aleatorio<20,200>() );

  // informa de que comienza a fumar
  mtx.lock();
  cout << "Fumador " << num_fumador << "  :"
         << " empieza a fumar (" << duracion_fumar.count() << " milisegundos)"
          << endl;
  mtx.unlock();

  // espera bloqueada un tiempo igual a ''duracion_fumar' milisegundos
  this_thread::sleep_for( duracion_fumar );

  // informa de que ha terminado de fumar
  mtx.lock();
  cout << "Fumador " << num_fumador
          << "  : termina de fumar, comienza espera de ingrediente." << endl;
  mtx.unlock();
}

//Monitor para regular la interaccion estanquero-fumador------------------------
class Estanco : public HoareMonitor{
private:
  int mostrador;                          //Mostrador vacio: -1; con ing_i = i
  CondVar c_est, c_fum[num_fumadores];

public:
  Estanco ();
  void ponerIngrediente(int i);
  void esperarMostradorVacio();
  void obtenerIngrediente(int i);
};

//Implementación de los métodos del monitor-------------------------------------

// Constructor
Estanco::Estanco(){
  mostrador = -1;
  c_est  = newCondVar();
  for (int i = 0; i < num_fumadores; i++) {
    c_fum[i] = newCondVar();
  }
}

void Estanco::ponerIngrediente(int i){
  mostrador = i;

  mtx.lock();
  std::cout << "Ingrediente en venta: " << i << endl;
  mtx.unlock();

  c_fum[i].signal();
}

void Estanco::esperarMostradorVacio(){
  if (mostrador != -1) {
    c_est.wait();
  }
}

void Estanco::obtenerIngrediente(int i){
  if (mostrador != i) {
    c_fum[i].wait();
  }
  std::cout << "Retirado ingrediente " << i << endl;

  mostrador = -1;
  c_est.signal();
}

//Funciones que realizan el trabajo de estanquero y fumadores-------------------

void hebra_estanquero(MRef<Estanco> estanco) {
  int ing;
  while (true) {
    ing = producirIngrediente();
    estanco->esperarMostradorVacio();
    estanco->ponerIngrediente(ing);
  }
}

void hebra_fumadora(MRef<Estanco> estanco, int i) {
  while (true) {
    estanco->obtenerIngrediente(i);
    fumar(i);
  }
}

//Programa principal------------------------------------------------------------

int main(int argc, char const *argv[]) {
  cout << "--------------------------" << endl
       << "Problema de los fumadores." << endl
       << "--------------------------" << endl;

  auto estanco = Create<Estanco>();

  thread estanquero(hebra_estanquero, estanco);
  thread fumadores[num_fumadores];
  for (size_t i = 0; i < num_fumadores; i++) {
    fumadores[i] = thread(hebra_fumadora, estanco, i);
  }

  estanquero.join();
  for (size_t i = 0; i < num_fumadores; i++) {
    fumadores[i].join();
  }
  return 0;
}
