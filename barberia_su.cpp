#include <iostream>
#include <iomanip>
#include <random>
#include <chrono>
#include <mutex>
#include "HoareMonitor.hpp"

using namespace HM;

//Variables globales------------------------------------------------------------
constexpr int
  num_clientes = 3;            // número de clientes
mutex
  mtx ;                        // mutex de escritura en pantalla

//Generador de números aleatorios-----------------------------------------------
template< int min, int max > int aleatorio(){
  static default_random_engine generador( (random_device())() );
  static uniform_int_distribution<int> distribucion_uniforme( min, max ) ;
  return distribucion_uniforme( generador );
}

//Funciones espera--------------------------------------------------------------
void esperarFueraBarberia(int i){
  chrono::milliseconds duracion_esperar( aleatorio<1000,1100>() );

  mtx.lock();
  std::cout << "Cliente" << i << ": Creciendole el pelo..." << endl;
  mtx.unlock();
  this_thread::sleep_for( duracion_esperar );
  mtx.lock();
  std::cout << "Cliente" << i << ": Me ha crecido el pelo, voy a pelarme" << endl;
  mtx.unlock();
}

void cortarPeloACliente(){
  chrono::milliseconds duracion_esperar( aleatorio<200,500>() );

  mtx.lock();
  std::cout << "Barbero: Pelado trapero en camino..." << endl;
  mtx.unlock();
  this_thread::sleep_for( duracion_esperar );
  mtx.lock();
  std::cout << "Barbero: Pelado trapero listo" << endl;
  mtx.unlock();
}

//Monitor para gestionar el acceso a una barbería-------------------------------
class Barberia : public HoareMonitor{
private:
  int num_esperando;                                    //Número de personas en la sala de espera
  bool cliente_en_la_silla;                             //Variable que indica si el barbero está pelando a alguien
  CondVar c_clientes, c_barbero, c_cliente_pelandose;   //Condiciones para barbero
public:
  Barberia();

  void siguienteCliente();
  void cortarPelo(int i);
  void finCliente();
};

//Implementación de los metodos de la barbería----------------------------------
Barberia::Barberia(){
  num_esperando = 0;
  cliente_en_la_silla = false;

  c_clientes = newCondVar();
  c_barbero = newCondVar();
  c_cliente_pelandose = newCondVar();
}

void Barberia::siguienteCliente(){
  if (num_esperando == 0) {                                                     //Si no hay ningun cliente, el barbero se duerme
    std::cout << "Barbero: No hay ningun cliente, me duermo zzz..." << endl;
    c_barbero.wait();
    std::cout << "Barbero: Buenos días zzz..." << endl;
  }
  std::cout << "Barbero: Que pase el siguiente cliente!" << endl;

  c_clientes.signal();                                                          //El barbero avisa al siguiente cliente para que pase
}

void Barberia::cortarPelo(int i) {
  std::cout << "Cliente" << i << ": Buenos dias!" << endl;
  c_barbero.signal();                                                           //El cliente despierta al barbero en caso de que este dormido

  std::cout << "Cliente" << i << ": Entro a la sala de espera" << endl;         //El cliente espera a que el barberlo le de paso
  num_esperando++;                                                              //El cliente notifica que está esperando
  c_clientes.wait();

  num_esperando--;
  std::cout << "Cliente" << i << ": Mi turno!" << endl;
  cliente_en_la_silla = true;
  c_cliente_pelandose.wait();                                                   //El cliente espera a que el barbero le pele
  std::cout << "Cliente" << i << ": Perfecto! Hasta luego!" << endl;
}

void Barberia::finCliente(){
  std::cout << "Barbero: Listo, le gusta como ha quedado?" << endl;
  cliente_en_la_silla = false;
  c_cliente_pelandose.signal();                                                 //El cliente ha sido pelado y sale de la barbería
}

//Funciones que realizan el trabajo de cliente y barbero------------------------
void hebra_cliente(MRef<Barberia> barberia, int i){
  while (true) {
    barberia->cortarPelo(i);        //Ir a cortarse el pelo
    esperarFueraBarberia(i);
  }
}

void hebra_barbero(MRef<Barberia> barberia){
  while (true) {
    barberia->siguienteCliente();
    cortarPeloACliente();
    barberia->finCliente();
  }
}

//Función principal-------------------------------------------------------------
int main(int argc, char const *argv[]) {
  cout << "------------------------" << endl
       << "Problema de la barberia." << endl
       << "------------------------" << endl;

  auto barberia = Create<Barberia>();

  thread barbero(hebra_barbero, barberia);
  thread clientes[num_clientes];
  for (size_t i = 0; i < num_clientes; i++) {
    clientes[i] = thread(hebra_cliente, barberia, i);
  }

  barbero.join();
  for (size_t i = 0; i < num_clientes; i++) {
    clientes[i].join();
  }
  return 0;
}
