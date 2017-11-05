
// *****************************************************************************
//
// C++ Hoare Monitors. Classes declarations.
// Carlos Ureña, Noviembre 2016
//
// This is a translation to modern C++ (C++11) of the Java code described here:
//   Better monitors for Java
//   By Theodore S. Norvell
//   http://www.javaworld.com/article/2077769/core-java/better-monitors-for-java.html
// without the need for "enter" or "leave" calls, which are automatially done by
// using the wrapper pattern (see below)
//
// Declarations.
//
// *****************************************************************************

#ifndef HOARE_MONITORS_HPP
#define HOARE_MONITORS_HPP

#include <iostream>
#include <mutex>
#include <condition_variable>
#include <cassert>
#include <vector>
#include <map>
#include <thread>  // thread
#include <memory> // shared_ptr, make_shared

// uncomment to get a log
//#define TRAZA_M

namespace HM
{

using namespace std ;
class HoareMonitor ;
class ThreadsQueue ;
template<class T> class Call_proxy ;

// *****************************************************************************
//
// Class: CondVar
//
// a class for conditions variables with "urgent wait" semantics on signal
// used in Hoare Monitors
// only to be used from  HoareMonitor class
//
// *****************************************************************************

class CondVar
{
   public:

   void     wait();     // unconditionally wait on the underlying thread queue
   void     signal();   // signal operation, with "urgent wait" semantics
   unsigned get_nwt() ; // returns number of threads waiting in the cond.var.

   bool empty() { return get_nwt() == 0 ; }

   // create an un-initialized condition variable, not usable
   CondVar();

   // --------------------------------------------------------------------------
   private:

   friend class HoareMonitor ; // forward declaration of friend monitor class
   HoareMonitor * monitor ;    // reference to the monitor for this variable
   unsigned       index ;      // index of the corresponding threads queue in
                               // the monitor threads queues vector

   // private constructor, only to be used from inside monitor implementation
   CondVar( HoareMonitor * p_monitor, unsigned p_index ) ;
};

// *****************************************************************************
//
// Class: HoareMonitor
//
// Base class for classic Hoare-style monitors
// (with "urgent wait" signal semantics)
//
// *****************************************************************************

class HoareMonitor
{
   public:

   // register calling thread name in the monitor
   void register_thread_name( const std::string & name );
   void register_thread_name( const std::string & rol, const int num );

   // get this thread registered name (or "unknown" if not registered)
   std::string get_thread_name()  ;

   // --------------------------------------------------------------------------
   protected:  // methods to be called from derived classes (concrete monitors)

   // constructors and destructor
   HoareMonitor() ;
   HoareMonitor( const std::string & p_name ) ;
   ~HoareMonitor();

   // create a new condition variable in this monitor
   CondVar newCondVar() ;

   // --------------------------------------------------------------------------
   private:

   // number of references (from MRef objects) pointing here
   // (must be atomic because it can be incremented/decremented by multiple threads)
   //atomic<int> reference_count ;

   // allow friend classes to access private parts of this class
   template<typename MonClass> friend class Call_proxy ;
   template<typename MonClass> friend class MRef ;
   friend class CondVar ;

   // name of this monitor (useful for debugging)
   std::string name ;

   // lock used for entering and exiting monitor queues,
   // guarantees a single total order for all operations (any thread on any queue)
   std::mutex queues_mtx ;

   // true iif any thread is running in the monitor
   bool running ;

   // identifier for thread currently in the monitor (when running==true)
   std::thread::id running_thread_id ;

   // queue for threads waiting to enter the monitor
   ThreadsQueue * monitor_queue ;

   // queue for threads waiting to re-enter the monitor after signal
   ThreadsQueue * urgent_queue ;

   // vector with all queues for user defined condition variables
   std::vector<ThreadsQueue *> queues ;

   // names map, updated in registerThreadName
   std::map< std::thread::id, std::string > names_map ;

   // mutex used to access the names map
   std::mutex names_mtx ;

   // enter and leave the monitor
   void enter();
   void leave();

   // initialize the monitor just after creation
   void initialize();

   // wait, signal and query on user-defined condition variables
   // (q_index is the index of the corresponding queue in the queues table)
   void     wait   ( unsigned q_index );
   void     signal ( unsigned q_index );
   unsigned get_nwt( unsigned q_index );

   // allow a waiting thread to enter the monitor
   void allow_another_to_enter() ;
} ;

// *****************************************************************************
extern std::mutex mcout ;

#ifdef TRAZA_M
#define logM( msg )     \
   {                   \
        mcout.lock();  \
        std::cout << msg << std::endl << std::flush ; \
        mcout.unlock(); \
   }
#else
#define logM( msg )
#endif

#define logEnt()   logM( "" << __FUNCTION__ << ": inicio (línea " << __LINE__ << ", archivo " << __FILE__ << ")" )

// *****************************************************************************
//
// Class: MRef
//
// reference to a monitor.
// all monitors should be accesed through these references
//
// implements the "execute around" pattern
// as described here:
//    Wrapping C++ Member Function Calls.
//    Bjarne Stroustrup
//    The C++ Report, June 2000.
//    http://www.stroustrup.com/wrapper.pdf
//
// *****************************************************************************

template<class MonClass> class MRef
{
   private:
   shared_ptr<MonClass> monPtr ; // shared pointer to the monitor

   public:

   // create a reference from a shared_ptr
   inline MRef( shared_ptr<MonClass>  p_monPtr )
   {
      assert( p_monPtr != nullptr );
      monPtr = p_monPtr ;
      logM( "inicio MRef( monitor * )  : rc == " << monPtr->reference_count  );
   }

   // obtain a call proxy through the dereference operator
   inline Call_proxy<MonClass>  operator -> ()
   {
     assert( monPtr != nullptr );
     return Call_proxy<MonClass>( *monPtr ) ; // acquires mutual exclusion
   }
} ;

// -----------------------------------------------------------------------------
// creation of a monitor reference by using a list of
// actual parameters (the list must match a monitor constructor parameters list)

template< class MonClass, class... Args > inline
MRef<MonClass> Create( Args &&... args )
{
   // equivalent to 'new'
   return MRef<MonClass>( make_shared<MonClass>( args... ) );
}

// *****************************************************************************
//
// Class Call_proxy<...>
//
// *****************************************************************************

template<class MonClass> class Call_proxy
{
   private:
   MonClass & monRef ; // monitor reference

   public:
   inline Call_proxy( MonClass & mr ) : monRef(mr) { monRef.enter(); }
   inline MonClass * operator -> () { return &monRef; }
   inline ~Call_proxy() { monRef.leave(); }
};

} // namespace HW end

// *****************************************************************************

#endif // ifndef MONITORESHSU_HPP
