#include <iostream>
#include <iomanip>
#include <random>
#include <chrono>
#include <mutex>
#include "HoareMonitor.hpp"

using namespace HM;

//Variables globales------------------------------------------------------------
constexpr int
  num_clientes = 7,            // número de clientes
  num_barberos = 2,            // número de barberos
  tamanio_sala = 5;
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
  chrono::milliseconds duracion_esperar( aleatorio<500,600>() );

  mtx.lock();
  std::cout << "                                    Cliente" << i << ": Creciendole el pelo..." << endl;
  mtx.unlock();
  this_thread::sleep_for( duracion_esperar );
  mtx.lock();
  std::cout << "                                    Cliente" << i << ": Me ha crecido el pelo, voy a pelarme" << endl;
  mtx.unlock();
}

void cortarPeloACliente(int i){
  chrono::milliseconds duracion_esperar( aleatorio<100,200>() );

  mtx.lock();
  std::cout << "Barbero"<< i << ": Pelando..." << endl;
  mtx.unlock();
  this_thread::sleep_for( duracion_esperar );
  mtx.lock();
  std::cout << "Barbero"<< i << ": Pelado listo" << endl;
  mtx.unlock();
}

//Monitor para gestionar el acceso a una barbería-------------------------------
class Barberia : public HoareMonitor{
private:
  CondVar c_clientes, c_barbero, c_cliente_pelandose;   //Condiciones
public:
  Barberia();

  void siguienteCliente(int i);
  void cortarPelo(int i);
  void finCliente(int i);
};

//Implementación de los metodos de la barbería----------------------------------
Barberia::Barberia(){
  c_clientes = newCondVar();
  c_barbero = newCondVar();
  c_cliente_pelandose = newCondVar();
}

void Barberia::siguienteCliente(int i){
  if (c_clientes.get_nwt() == 0) {                                                     //Si no hay ningun cliente, el barbero se duerme
    mtx.lock();
    std::cout << "Barbero"<< i << ": No hay ningun cliente, me duermo zzz..." << endl;
    mtx.unlock();
    c_barbero.wait();
    mtx.lock();
    std::cout << "Barbero"<< i << ": Buenos días zzz... Pase pase" << endl;
    mtx.unlock();
  }
  else{
    mtx.lock();
    std::cout << "Barbero"<< i << ": Que pase el siguiente cliente!" << endl;
    mtx.unlock();
    c_clientes.signal();
  }                                                          //El barbero avisa al siguiente cliente para que pase
}

void Barberia::cortarPelo(int i) {
  mtx.lock();
  std::cout << "                                    Cliente" << i << ": Buenos dias!" << endl;
  mtx.unlock();
  if (c_barbero.get_nwt() != 0)
    c_barbero.signal();                                                         //El cliente despierta al barbero en caso de que este dormido
  else{
    if (c_clientes.get_nwt() >= tamanio_sala) {
      mtx.lock();
      std::cout << "                                   Cliente" << i << ": Hay mucha cola, vuelvo luego!" << '\n';
      mtx.unlock();
      return;
    }
    mtx.lock();
    std::cout << "                                    Cliente" << i << ": Entro a la sala de espera" << endl;         //El cliente espera a que el barberlo le de paso                                                            //El cliente notifica que está esperando
    mtx.unlock();
    c_clientes.wait();
  }
  mtx.lock();
  std::cout << "                                    Cliente" << i << ": Pelándose..." << endl;
  mtx.unlock();
  c_cliente_pelandose.wait();                                                   //El cliente espera a que el barbero le pele
  mtx.lock();
  std::cout << "                                    Cliente" << i << ": Perfecto! Hasta luego!" << endl;
  mtx.unlock();
}

void Barberia::finCliente(int i){
  mtx.lock();
  std::cout << "Barbero"<< i << ": Listo, le gusta como ha quedado?" << endl;
  mtx.unlock();
  c_cliente_pelandose.signal();                                                 //El cliente ha sido pelado y sale de la barbería
}

//Funciones que realizan el trabajo de cliente y barbero------------------------
void hebra_cliente(MRef<Barberia> barberia, int i){
  while (true) {
    barberia->cortarPelo(i);        //Ir a cortarse el pelo
    esperarFueraBarberia(i);
  }
}

void hebra_barbero(MRef<Barberia> barberia, int i){
  while (true) {
    barberia->siguienteCliente(i);
    cortarPeloACliente(i);
    barberia->finCliente(i);
  }
}

//Función principal-------------------------------------------------------------
int main(int argc, char const *argv[]) {
  mtx.lock();
  cout << "------------------------" << endl
       << "Problema de la barberia." << endl
       << "------------------------" << endl;
  mtx.unlock();
  auto barberia = Create<Barberia>();

  thread barberos[num_barberos];
  thread clientes[num_clientes];
  for (size_t i = 0; i < num_barberos; i++) {
    barberos[i] = thread(hebra_barbero, barberia, i);
  }
  for (size_t i = 0; i < num_clientes; i++) {
    clientes[i] = thread(hebra_cliente, barberia, i);
  }

  for (size_t i = 0; i < num_barberos; i++) {
    barberos[i].join();
  }
  for (size_t i = 0; i < num_clientes; i++) {
    clientes[i].join();
  }
  return 0;
}
