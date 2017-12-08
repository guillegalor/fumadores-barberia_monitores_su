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
  max_clientes = 3,            // número maximo de clientes que puede despachar un barbero sin descansar
  tamanio_sala = 5;            // número maximo de clientes esperando en la sala de espera
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
  std::cout << std::string( 15, ' ' )
    << " Cliente" << i
      << ": Creciendole el pelo..."
        << endl;
  mtx.unlock();
  this_thread::sleep_for( duracion_esperar );
  mtx.lock();
  std::cout << std::string( 15, ' ' )
    << " Cliente" << i
      << ": Me ha crecido el pelo, voy a pelarme"
        << endl;
  mtx.unlock();
}

void cortarPeloACliente(int i){
  chrono::milliseconds duracion_esperar( aleatorio<100,200>() );

  mtx.lock();
  std::cout << "Barbero"<< i
    << ": Pelando..." << endl;
  mtx.unlock();
  this_thread::sleep_for( duracion_esperar );
  mtx.lock();
  std::cout << "Barbero"<< i
    << ": Pelado listo" << endl;
  mtx.unlock();
}

//Monitor para gestionar el acceso a una barbería-------------------------------
class Barberia : public HoareMonitor{
private:
  int siguiente_barbero;
  unsigned clientes_x_barbero[num_barberos];
  CondVar c_clientes, c_barbero, c_cliente_pelandose[num_barberos];   //Condiciones
public:
  Barberia();

  void siguienteCliente(int i);
  void cortarPelo(int i);
  bool finCliente(int i);
};

//Implementación de los metodos de la barbería----------------------------------
Barberia::Barberia(){
  siguiente_barbero = -1;
  for (size_t i = 0; i < num_barberos; i++) {
    clientes_x_barbero[i] = 0;
    c_cliente_pelandose[i] = newCondVar();
  }
  c_clientes = newCondVar();
  c_barbero = newCondVar();
}

void Barberia::siguienteCliente(int i){
  if (c_clientes.get_nwt() == 0) {                          //Si no hay ningun cliente, el barbero se duerme
    mtx.lock();
    std::cout << "Barbero" << i
      << ": No hay ningun cliente, me duermo zzz..."
        << endl;
    mtx.unlock();
    c_barbero.wait();
    mtx.lock();
    std::cout << "Barbero" << i
      << ": Buenos días zzz... Pase pase"
        << endl;
    mtx.unlock();
    siguiente_barbero = i;
  }
  else{
    mtx.lock();
    std::cout << "Barbero" << i
      << ": Que pase el siguiente cliente!"
        << endl;
    mtx.unlock();
    siguiente_barbero = i;
    c_clientes.signal();                                    //El barbero avisa al siguiente cliente para que pase
  }
}

void Barberia::cortarPelo(int i) {
  mtx.lock();
  std::cout << std::string( 15, ' ' )
    << " Cliente" << i
      << ": Buenos dias!" << endl;
  mtx.unlock();
  if (c_barbero.get_nwt() != 0)
    c_barbero.signal();                                     //El cliente despierta al barbero en caso de que este dormido
  else{
    if (c_clientes.get_nwt() >= tamanio_sala) {
      mtx.lock();
      std::cout << std::string( 15, ' ' )
        << "Cliente" << i
          << ": Hay mucha cola, vuelvo luego!"
            << endl;
      mtx.unlock();
      return;
    }
    mtx.lock();
    std::cout << std::string( 15, ' ' )
      << " Cliente" << i
        << ": Entro a la sala de espera"
          << endl;                                          //El cliente espera a que el barberlo le de paso                                                            //El cliente notifica que está esperando
    mtx.unlock();
    c_clientes.wait();
  }
  mtx.lock();
  std::cout << std::string( 15, ' ' )
    << " Cliente" << i << ": Pelándose..."
      << endl;
  mtx.unlock();
  c_cliente_pelandose[siguiente_barbero].wait();               //El cliente espera a que el barbero le pele
  mtx.lock();
  std::cout << std::string( 15, ' ' )
    << " Cliente" << i
      << ": Perfecto! Hasta luego!"
        << endl;
  mtx.unlock();
}

bool Barberia::finCliente(int i){
  clientes_x_barbero[i]++;
  mtx.lock();
  std::cout << "Barbero"<< i
    << ": Listo, le gusta como ha quedado?"
      << endl;
  mtx.unlock();
  c_cliente_pelandose[i].signal();                             //El cliente ha sido pelado y sale de la barbería

  if(clientes_x_barbero[i] >= max_clientes){
    clientes_x_barbero[i] = 0;
    return true;
  }
  else
    return false;
}

//Funciones que realizan el trabajo de cliente y barbero------------------------
void hebra_cliente(MRef<Barberia> barberia, int i){
  while (true) {
    barberia->cortarPelo(i);                                //Ir a cortarse el pelo
    esperarFueraBarberia(i);
  }
}

void hebra_barbero(MRef<Barberia> barberia, int i){
  while (true) {
    barberia->siguienteCliente(i);
    cortarPeloACliente(i);
    if(barberia->finCliente(i)){
      mtx.lock();
      std::cout << "Barbero" << i
        << ": Estoy muy cansado, voy a descansar un ratito"
          << endl;
      mtx.unlock();
      this_thread::sleep_for(std::chrono::seconds(2));

      mtx.lock();
      std::cout << "Barbero" << i
        << ": Ya he descansado, a trabajar!"
          << endl;
    }
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
