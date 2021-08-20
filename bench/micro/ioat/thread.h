#ifndef CTHRAED_H
#define CTHRAED_H
extern "C" {   
#include <pthread.h>
}
#include<string>

using namespace std;

/**
 *   Signals a problem with the thread handling.
 */

class CThreadException: public std::exception
{
	public:
		/**
		 *   Construct a SocketException with a explanatory message.
		 *   @param message explanatory message
		 *   @param bSysMsg true if system message (from strerror(errno))
		 *   should be postfixed to the user provided message
		 */
		CThreadException(const string &message, bool bSysMsg = false) throw();


		/** Destructor.
		 * Virtual to allow for subclassing.
		 */
		virtual ~CThreadException() throw ();

		/** Returns a pointer to the (constant) error description.
		 *  @return A pointer to a \c const \c char*. The underlying memory
		 *          is in posession of the \c Exception object. Callers \a must
		 *          not attempt to free the memory.
		 */
		virtual const char* what() const throw (){  return m_sMsg.c_str(); }

	protected:
		/** Error message.
		*/
		std::string m_sMsg;
};


/** 
 *  Abstract class for Thread management 
 */
class CThread
{
	public:
		int done;

		/**
		 *   Default Constructor for thread
		 */
		CThread();

		/**
		 *   virtual destructor  
		 */
		virtual ~CThread();

		/**
		 *   Thread functionality Pure virtual function  , it will be re implemented in derived classes  
		 */
		virtual void Run() = 0;

		/**
		 *   Function to start thread. 
		 */
		void Start() throw(CThreadException);

		/**
		 *   Function to join thread. 
		 */
		void Join() throw(CThreadException);
	private:

		/**
		 *   private Function to create thread. 
		 */
		void CreateThread() throw(CThreadException);

		/**
		 *   Call back Function Passing to pthread create API
		 */
		static void* ThreadFunc(void* pTr);

		/**
		 *   Internal pthread ID.. 
		 */
		pthread_t m_Tid;
};
#endif
