/**
 * @file      lib.SharedPtr.hpp
 * @author    Sergey Baigudin, sergey@baigudin.software
 * @copyright 2020-2021, Sergey Baigudin, Baigudin Software
 */
#ifndef LIB_SHARED_PTR_HPP_
#define LIB_SHARED_PTR_HPP_

#include "lib.Object.hpp"
#include "lib.MutexGuard.hpp"

namespace eoos
{
namespace lib
{

#ifdef EOOS_NO_STRICT_MISRA_RULES

/**
 * @class SharedPtrDeleter<T>
 * @brief Deleter of shared pointers allocate with new operator.
 *
 * @tparam T Data type of an owning object. 
 */
template <typename T>
class SharedPtrDeleter
{
    
public:

    /**
     * @brief Frees an allocated object.
     *
     * @param ptr Address of allocated the owning object.
     */
    static void free(T* const ptr)
    {
        delete ptr;
    }
};

/**
 * @class SharedPtrDeleterArray<T>
 * @brief Deleter of shared pointers allocate with new [] operator.
 *
 * @tparam T Data type of an owning object. 
 */
template <typename T>
class SharedPtrDeleterArray
{
    
public:

    /**
     * @brief Frees an allocated array of objects.
     *
     * @param ptr Address of allocated the owning objects.
     */
    static void free(T* const ptr)
    {
        delete [] ptr;
    }
};
    
/**
 * @class SharedPtr<T,D,A,M>
 * @brief Shared pointer.
 *
 * @tparam T Data type of an owning object.
 * @tparam D Deleter type for an owning object. 
 * @tparam A Heap memory allocator class.
 * @tparam M Mutex to protect the control block inherited from @ref api::Mutex.
 */
template <typename T, class D = SharedPtrDeleter<T>, class A = Allocator, class M = Mutex<A> >
class SharedPtr : public Object<A>
{
    typedef SharedPtr<T,A>  Self;
    typedef Object<A> Parent;

public:

    /**
     * @brief Constructor an empty shared object.
     */
    SharedPtr() : Parent(),
        cb_ (NULLPTR){
        bool_t const isConstructed = construct();
        setConstructed(isConstructed);    
    }

    /**
     * @brief Constructor.
     *
     * @param pointer A pointer to get ownership.
     * @note If the shared object is not able to be constructed, an object passed by the pointer will be deleted.
     */
    explicit SharedPtr(T* const pointer) : Parent(),
        cb_ (NULLPTR){
        bool_t const isConstructed = construct(pointer);
        setConstructed(isConstructed);    
    }

    /**
     * @brief Destructor.
     */
    virtual ~SharedPtr()
    {
        if( isConstructed() )
        {
            release();
        }
    }
    
    /**
     * @copydoc eoos::Object::Object(const Object&)
     */
    SharedPtr(const SharedPtr& obj) : Parent(obj),
        cb_ (obj.cb_){
        acquire();
    }
    
    /**
     * @copydoc eoos::Object::operator=(const Object&)
     */       
    SharedPtr& operator=(const SharedPtr& obj)
    {
        if( this != &obj && isConstructed() )
        {
            release();
            Parent::operator=(obj);            
            cb_ = obj.cb_;
            acquire();
        }
        return *this;
    }    

    #if EOOS_CPP_STANDARD >= 2011

    /**
     * @copydoc eoos::Object::Object(const Object&&)    
     */       
    SharedPtr(SharedPtr&& obj) noexcept : Parent( move(obj) ),
        cb_ (obj.cb_){
    }   
    
    /**
     * @copydoc eoos::Object::operator=(const Object&&)
     */
    SharedPtr& operator=(SharedPtr&& obj) noexcept
    {
        if( this != &obj && isConstructed() )
        {
            Parent::operator=( move(obj) );            
            cb_ = obj.cb_;
        }        
        return *this;
    }        
    
    #endif // EOOS_CPP_STANDARD >= 2011

    /**
     * @brief Casts to boolean data type comparing if the stored pointer does not equal to null.
     *
     * @return Comparation the stored pointer does not equal to null.
     */    
    operator bool_t() const 
    {
        return get() != NULLPTR;
    }
    
    /**
     * @brief Returns the result of dereferencing the stored pointer.
     *
     * @return The dereferenced stored pointer.
     */
    T& operator*() const
    {
        return *get();
    }
    
    /**
     * @brief Returns the stored pointer.
     *
     * @return The stored pointer or NULLPTR if no pointer stored.
     */
    T* operator->() const
    {
        return get();
    }
    
    /**
     * @brief Returns an element of the stored array.
     *
     * @param index An element index.
     * @return An element.
     */    
    T& operator[](uint32_t const index) const
    {
        T* pointer = get();
        return pointer[index];
    }
    
    /**
     * @brief Returns the stored pointer.
     *
     * @return The stored pointer or NULLPTR if no pointer stored.
     */   
    T* get() const
    {
        T* pointer = NULLPTR;
        if( isConstructed() )
        {
            pointer = cb_->getPointer();
        }
        return pointer;
    }
    
    /**
     * @brief Returns counter of shared objects for the managed object.
     *
     * @return Counter of shared objects.
     */   
    int32_t getCount() const
    {
        int32_t counter = 0;
        if( isConstructed() )
        {
            counter = cb_->getCounter();
        }
        return counter;
    }
        
private:

    /**
     * @brief Constructs this object.
     *
     * @param pointer A pointer to get ownership.
     * @return True if this object has been constructed successfully.
     */     
    bool_t construct(T* const pointer = NULLPTR)
    {
        bool_t res = false;
        do
        {
            if( not isConstructed() )
            {
                D::free(pointer);
                break;
            }
            cb_ = new ControlBlock<T,D,A,M>(pointer);
            if(cb_ == NULLPTR)
            {
                D::free(pointer);
                break;
            }
            if( not cb_->isConstructed() )
            {
                delete cb_;
                cb_ = NULLPTR;
                break;
            }
            res = true;
        } while(false);
        return res;
    }
    
    /**
     * @brief Releases the managed object by control block.
     */       
    void release()
    {
        if( cb_ != NULLPTR )
        {
            uint32_t const counter = cb_->decrease();
            if(counter == 0)
            {
                delete cb_;
                cb_ = NULLPTR;            
            }
        }
    }        
    
    /**
     * @brief Acquires a managed object by control block.
     */
    void acquire()
    {
        if( cb_ != NULLPTR )
        {
            cb_->increase();
        }
        else
        {
            setConstructed(false);
        }
    }
    
    /**
     * @class ControlBlock<TT,DD,AA,MM>
     * @brief Primary template implementation of shared pointer control block class.
     *
     * @tparam TT Data type of owning the object.
     * @tparam DD Deleter type for owning the object.  
     * @tparam AA Heap memory allocator class.
     * @tparam MM Mutex to protect the control block inherited from @ref api::Mutex.
     *
     * @note This class is implemented as an auxiliry class for SharedPtr
     * and is tested for construction before usage. Therefore, some checks
     * are skipped in public interface to speedup performence. 
     */
    template <typename TT, class DD, class AA, class MM> 
    class ControlBlock : public Object<AA>
    {
        typedef Object<AA> Parent;    
        
    public:
        
        /**
         * @brief Constructor.
         *
         * @param pointer A pointer to get ownership.
         */
        explicit ControlBlock(T* const pointer) : Parent(),
            pointer_ (pointer),
            counter_ (1),
            mutex_   (){
            bool_t const isConstructed = construct();
            setConstructed(isConstructed);
        }
        
        /**
         * @brief Destructor.
         */
        virtual ~ControlBlock()
        {
            DD::free(pointer_);
        }    
        
        /**
         * @brief Increases the counter on one.
         */
        void increase()
        {
            MutexGuard<AA> const guard(mutex_);
            ++counter_;
        }
        
        /**
         * @brief Decreases the counter on one.
         *
         * @return A value of the counter after decreasing.
         */        
        int32_t decrease()
        {
            MutexGuard<AA> const guard(mutex_);        
            return --counter_;
        }
        
        /**
         * @brief Returns the counter.
         *
         * @return A value of the counter.
         */        
        int32_t getCounter() const
        {    
            return counter_;
        }
        
        /**
         * @brief Returns the counter.
         *
         * @return A value of the counter.
         */        
        TT* getPointer() const
        {
            return pointer_;
        }    
    
    private:
    
        /**
         * @brief Constructs this object.
         */     
        bool_t construct()
        {
            bool_t res = false;
            do
            {
                if( not isConstructed() )
                {
                    break;
                }
                if( not mutex_.isConstructed() )
                {
                    break;
                }
                res = true;
            } while(false);
            return res;
        }
    
        /**
         * @copydoc eoos::Object::Object(const Object&)
         */
        ControlBlock(const ControlBlock& obj);
        
        /**
         * @copydoc eoos::Object::operator=(const Object&)
         */       
        ControlBlock& operator=(const ControlBlock& obj);
    
        #if EOOS_CPP_STANDARD >= 2011
    
        /**
         * @copydoc eoos::Object::Object(const Object&&)
         */       
        ControlBlock(ControlBlock&& obj) noexcept = delete; 
        
        /**
         * @copydoc eoos::Object::operator=(const Object&&)
         */
        ControlBlock& operator=(ControlBlock&& obj) noexcept = delete;
        
        #endif // EOOS_CPP_STANDARD >= 2011
    
        /**
         * @brief An owned pointer.
         */
        TT* pointer_;
        
        /**
         * @brief Counter of copies of the shared objects.
         */
        int32_t counter_;
    
        /**
         * @brief Mutex to protect the counter.
         */    
        MM mutex_;
    };
    
    /**
     * @brief Control block of the managed object.
     */
    ControlBlock<T,D,A,M>* cb_;

};

#endif // EOOS_NO_STRICT_MISRA_RULES

} // namespace lib
} // namespace eoos
#endif // LIB_SHARED_PTR_HPP_
