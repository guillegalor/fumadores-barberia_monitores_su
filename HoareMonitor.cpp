// *****************************************************************************
//
// Hoare Monitors in C++11
// Carlos Ureña, November 2016, March 2017
//
// classes declarations
//
// *****************************************************************************


#include <iostream>
#include <cassert>
#include <thread>  // incluye std::this_thread::get_id()
#include <system_error>
#include "HoareMonitor.hpp"

namespace HM
{

std::mutex mcout ;
using namespace std ;

// *****************************************************************************
//
// Class ThreadsQueue
//
//
// A thread queue with two states: close or open
// Equivalent to a binary semaphore, with values 0 (closed) or 1 (open)
// Operations:
//      wait   : if closed then do blocked wait, if open do not wait and close
//      signal : if any thread is waiting then signal one, if none is waiting then just open

class ThreadsQueue
{
   private:

   bool                      open ;   // current state
   std::condition_variable   queue ;  // queue with waiting threads
   unsigned                  num_wt ; // current number of waiting threads

   public:

   ThreadsQueue( bool p_open ) ;

   void     wait( std::unique_lock<std::mutex> & lock );
   bool     signal();
   unsigned get_nwt() const;

} ;

// *****************************************************************************
//  ThreadQueue (binary semaphore)

ThreadsQueue::ThreadsQueue( bool p_open )
{

  open = p_open ;
  num_wt = 0 ;
}
// -----------------------------------------------------------------------------

unsigned ThreadsQueue::get_nwt(  ) const
{
  return num_wt ;
}
// -----------------------------------------------------------------------------
// wait operation: the caller thread must own the lock
// if the queue is "closed" (open==false),
//        the caller releases the lock and blocks
//        when it is waked up, it waits to reacquire the lock
//        the queue reamins closed, the caller ends this call
// if the queue is "opened" (open==true)
//        the caller does not block, ends this call
//        the queue is closed (open=false)
//
// (the caller thread must own the mutex in the parameter, later this mutex
// must be owned by the thread signalling this queue)

void ThreadsQueue::wait( std::unique_lock<std::mutex> & lock )
{
  // wait while value is false (must be 'while' because of possible spurious wakeups )
  num_wt += 1 ;       // one more waiting thread
  while ( not open )  // use of while (instead of 'if') avoids errors due to spurious wake-ups
    queue.wait( lock );
  num_wt -= 1 ;       // one less waiting thread

  // close the queue
  assert( open );
  open = false ;
}
// -----------------------------------------------------------------------------
// signal operation
//    opens the queue (set value="true"), then:
//    if there was any waiting thread
//       caller thread ends the call, returns true
//       a thread is waked-up, this thread closes the queue and continues
//    if there was no waiting thread
//       returns false
//

bool ThreadsQueue::signal( )
{
  // open the queue
  open = true ;

  // signal one waiting thread, if any
  if ( 0 < num_wt )
  {
    queue.notify_one() ;  // wake up one waiting thread
    return true ;         // free mutex, end, after return queue will be closed by waked up thread
  }
  else
    return false ;          // free mutex, end, queue remains open
}

// *****************************************************************************
//
// Class: CondVar
//
// *****************************************************************************

// create an unusable, not initialized condition variable

CondVar::CondVar()
{
   monitor = nullptr ;
}
// -----------------------------------------------------------------------------
//  create an usable condition variable

CondVar::CondVar( HoareMonitor * p_monitor, unsigned p_index )
{
   assert( p_monitor != nullptr );
   assert( p_index == (p_monitor->queues).size()-1 );

   monitor = p_monitor ;
   index   = p_index ;
}
// -----------------------------------------------------------------------------
// unconditionally wait on the underlying thread queue

void CondVar::wait()
{
   assert( monitor != nullptr );
   monitor->wait( index ) ;
}
// -----------------------------------------------------------------------------
// signal operation, with "urgent wait" semantics

void CondVar::signal()
{
   assert( monitor != nullptr );
   monitor->signal( index );
}
// -----------------------------------------------------------------------------
// returns number of threads waiting in the cond.var.

unsigned CondVar::get_nwt()
{
   assert( monitor != nullptr );
   return monitor->get_nwt( index );
}

// *****************************************************************************
//
// Class: HoareMonitor
//
// *****************************************************************************

void HoareMonitor::initialize()
{
   running         = false ;
   //reference_count = 0 ;
   urgent_queue    = new ThreadsQueue( false );  // initially (and always) closed
   monitor_queue   = new ThreadsQueue( true ); // initially open
}
// -----------------------------------------------------------------------------
HoareMonitor::HoareMonitor()
{
   name = "unknown" ;
   initialize();
}
// -----------------------------------------------------------------------------

HoareMonitor::HoareMonitor( const std::string & p_name )
{
   name = p_name ;
   initialize();
}
// -----------------------------------------------------------------------------
HoareMonitor::~HoareMonitor()
{
   //cout << "starts monitor destructor" << endl ;

   assert( ! running );

   // destroy all threads queues
   for( auto & tq_ptr : queues )
   {
      assert( tq_ptr != nullptr );
      assert( tq_ptr->get_nwt() == 0 );
      delete tq_ptr ;
      tq_ptr = nullptr ;
   }

   assert( urgent_queue != nullptr );
   assert( urgent_queue->get_nwt() == 0 );
   delete urgent_queue ;
   urgent_queue = nullptr ;

   assert( monitor_queue != nullptr );
   assert( monitor_queue->get_nwt() == 0 );
   delete monitor_queue ;
   monitor_queue = nullptr ;


   //cout << "ends monitor destructor" << endl ;
}
// -----------------------------------------------------------------------------

CondVar HoareMonitor::newCondVar()
{
   queues.push_back( new ThreadsQueue( false ) ); // add threads queue to monitor
   return CondVar( this, queues.size()-1 );       // built and return cond.var.
}

// -----------------------------------------------------------------------------
// enter the monitor, waiting if neccesary

void HoareMonitor::enter()
{
  // acquire queues access mutex
  std::unique_lock<std::mutex> lock( queues_mtx );

  // wait if the monitor queue is closed (other thread is running the monitor)
  monitor_queue->wait( lock );

  assert( ! running );
  // register this thread is running in the monitor
  running = true ;
  running_thread_id = std::this_thread::get_id();

  // release queues access mutex (destroy 'lock')
}
// -----------------------------------------------------------------------------
// end running monitor code

void HoareMonitor::leave()
{
  // check this is the thread running in the monitor
  assert( running );
  assert( std::this_thread::get_id() == running_thread_id );

  // acquire queues access mutex
  std::unique_lock<std::mutex> lock( queues_mtx );

  // register no thread is running in the monitor
  running = false ;

  // allow another thread to start or continue running in the monitor, if any is waiting
  allow_another_to_enter();

  // release queues access mutex (destroy 'lock')
}
// -----------------------------------------------------------------------------
// allow a waiting thread to enter the monitor, if any

void HoareMonitor::allow_another_to_enter()
{
  if ( 0 < urgent_queue->get_nwt() )  // if any thread in the urgent queue
     urgent_queue->signal();          //   release one, allow it to enter (remains closed)
  else                                // if no thread in the urgent
     monitor_queue->signal();         //   signal the monitor queue
}
// -----------------------------------------------------------------------------
// wait on a queue

void HoareMonitor::wait( unsigned q_index )
{
   // check this is the thread running in the monitor
   assert( running );
   assert( std::this_thread::get_id() == running_thread_id );

   // check 'q_index' is a valid queue index
   assert( q_index < queues.size() );

   // acquire queues access mutex
   std::unique_lock<std::mutex> lock( queues_mtx );

   // allow another thread to start or continue running in the monitor, if any is waiting
   // (that thread, if any, cannot run until this thread releases 'queues_mtx' when this starts waiting)
   allow_another_to_enter();

   // register no thread is running in the monitor
   running = false ;

   // blocked wait on the condition threads queue
   queues[q_index]->wait( lock );

   // check the signaling thread did set running to true
   assert( running );

   // re-enter the monitor: register this is the thread running in the monitor
   running_thread_id = std::this_thread::get_id();

   // release queues access mutex (destroy 'lock')
}

// -----------------------------------------------------------------------------
// signal with urgent wait semantics on a user-defined variable condition

void HoareMonitor::signal( unsigned q_index )
{
   assert( running );
   assert( std::this_thread::get_id() == running_thread_id );
   assert( q_index < queues.size() );

   // wait to get the queues lock, then acquire it.
   std::unique_lock<std::mutex> lock( queues_mtx );

   // does nothing when queue is empty
   if ( 0 < queues[q_index]->get_nwt() )
   {
      // signal a thread in the queue, it cannot run yet
      queues[q_index]->signal() ;

      // 1. release queues mutex (allows signalled thread to run),
      // 2. wait for signalled thread to stop running in the monitor
      // 3. reacquire de queues lock
      urgent_queue->wait( lock );

      // check that the signalled thread did set 'running' to false when exited or entered a queue)
      assert( ! running );

      // register this is the running thread
      running = true ;
      running_thread_id = std::this_thread::get_id();
   }
   // release queues lock (destroy 'lock').
}
// -----------------------------------------------------------------------------
// returns number of waiting threads in a queue (associated to a user-defined cv)

unsigned HoareMonitor::get_nwt( unsigned q_index )
{
  assert( running );
  assert( std::this_thread::get_id() == running_thread_id );
  assert( q_index < queues.size() );

  std::unique_lock<std::mutex> lock( queues_mtx );
  return queues[q_index]->get_nwt() ;
}
// -----------------------------------------------------------------------------
// register calling thread name in the monitor, useful for debugging

void HoareMonitor::register_thread_name( const std::string & rol, const int number )
{
  const std::string name = rol + " " + std::to_string(number) ;
  register_thread_name( name );
}
// -----------------------------------------------------------------------------

void HoareMonitor::register_thread_name( const std::string & name )
{
  std::unique_lock<std::mutex> lock( names_mtx );
  // get thread id in ttid
  const std::thread::id ttid = std::this_thread::get_id() ; // this thread ident.

  // search and abort if already registered
  const auto iter = names_map.find( ttid );
  if ( iter != names_map.end() )
  {
    logM("that id was already registered, with name == '" << iter->second << "', aborting");
    exit(1);
  }

  // insert name
  names_map.insert( std::pair< std::thread::id, std::string>( ttid, name ) );
}

// -----------------------------------------------------------------------------
// get this thread registered name (or "unknown" if not registered)
std::string HoareMonitor::get_thread_name()
{
  std::unique_lock<std::mutex> lock( names_mtx );

  const auto ttid = std::this_thread::get_id() ; // this thread ident.
  const auto iter = names_map.find( ttid );

  if ( iter != names_map.end() )
    return iter->second ;
  else
    return "(unknown)" ;

}




// *****************************************************************************


} //  fin namespace HoareMonitors
