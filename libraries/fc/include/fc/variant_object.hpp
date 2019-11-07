#pragma once
#include <fc/variant.hpp>
#include <fc/shared_ptr.hpp>
#include <fc/unique_ptr.hpp>

namespace fc
{
   class mutable_variant_object;
   
   /**
    *  @ingroup Serializable
    *
    *  @brief An order-perserving dictionary of variant's.  
    *
    *  Keys are kept in the order they are inserted.
    *  This dictionary implements copy-on-write
    *
    *  @note This class is not optimized for random-access on large
    *        sets of key-value pairs.
    */
   class variant_object
   {
   public:
      /** @brief a key/value pair */
      class entry 
      {
      public:
         entry();
         entry( string k, fc::variant v );
         entry( entry&& e );
         entry( const entry& e);
         entry& operator=(const entry&);
         entry& operator=(entry&&);
                
         const string&        key()const;
         const fc::variant& value()const;
         void  set( fc::variant v );

         fc::variant&       value();
             
      private:
         string  _key;
         fc::variant _value;
      };

      typedef std::vector< entry >::const_iterator iterator;

      /**
         * @name Immutable Interface
         *
         * Calling these methods will not result in copies of the
         * underlying type.
         */
      ///@{
      iterator begin()const;
      iterator end()const;
      iterator find( const string& key )const;
      iterator find( const char* key )const;
      const fc::variant& operator[]( const string& key )const;
      const fc::variant& operator[]( const char* key )const;
      size_t size()const;
      bool   contains( const char* key ) const { return find(key) != end(); }
      ///@}

      variant_object();

      /** initializes the first key/value pair in the object */
      variant_object( string key, fc::variant val );
       
      template<typename T>
      variant_object( string key, T&& val )
      :_key_value( std::make_shared<std::vector<entry> >() )
      {
         *this = variant_object( std::move(key), fc::variant(fc::forward<T>(val)) );
      }
      variant_object( const variant_object& );
      variant_object( variant_object&& );

      variant_object( const mutable_variant_object& );
      variant_object( mutable_variant_object&& );

      variant_object& operator=( variant_object&& );
      variant_object& operator=( const variant_object& );

      variant_object& operator=( mutable_variant_object&& );
      variant_object& operator=( const mutable_variant_object& );

   private:
      std::shared_ptr< std::vector< entry > > _key_value;
      friend class mutable_variant_object;
   };
   /** @ingroup Serializable */
   void to_variant( const variant_object& var, fc::variant& vo, uint32_t max_depth = 1 );
   /** @ingroup Serializable */
   void from_variant( const fc::variant& var, variant_object& vo, uint32_t max_depth = 1 );


  /**
   *  @ingroup Serializable
   *
   *  @brief An order-perserving dictionary of variant's.  
   *
   *  Keys are kept in the order they are inserted.
   *  This dictionary implements copy-on-write
   *
   *  @note This class is not optimized for random-access on large
   *        sets of key-value pairs.
   */
   class mutable_variant_object
   {
   public:
      /** @brief a key/value pair */
      typedef variant_object::entry  entry;

      typedef std::vector< entry >::iterator       iterator;
      typedef std::vector< entry >::const_iterator const_iterator;

      /**
         * @name Immutable Interface
         *
         * Calling these methods will not result in copies of the
         * underlying type.
         */
      ///@{
      iterator begin()const;
      iterator end()const;
      iterator find( const string& key )const;
      iterator find( const char* key )const;
      const fc::variant& operator[]( const string& key )const;
      const fc::variant& operator[]( const char* key )const;
      size_t size()const;
      ///@}
      fc::variant& operator[]( const string& key );
      fc::variant& operator[]( const char* key );

      /**
         * @name mutable Interface
         *
         * Calling these methods will result in a copy of the underlying type 
         * being created if there is more than one reference to this object.
         */
      ///@{
      void                 reserve( size_t s);
      iterator             begin();
      iterator             end();
      void                 erase( const string& key );
      /**
         *
         * @return end() if key is not found
         */
      iterator             find( const string& key );
      iterator             find( const char* key );


      /** replaces the value at \a key with \a var or insert's \a key if not found */
      mutable_variant_object& set( string key, fc::variant var );
      /** Appends \a key and \a var without checking for duplicates, designed to
         *  simplify construction of dictionaries using (key,val)(key2,val2) syntax 
         */
      /**
      *  Convenience method to simplify the manual construction of
      *  variant_object's
      *
      *  Instead of:
      *    <code>mutable_variant_object("c",c).set("a",a).set("b",b);</code>
      *
      *  You can use:
      *    <code>mutable_variant_object( "c", c )( "b", b)( "c",c )</code>
      *
      *  @return *this;
      */
      mutable_variant_object& operator()( string key, fc::variant var, uint32_t max_depth = 1 );
      template<typename T>
      mutable_variant_object& operator()( string key, T&& var, uint32_t max_depth )
      {
         _FC_ASSERT( max_depth > 0, "Recursion depth exceeded!" );
         set( std::move(key), fc::variant( fc::forward<T>(var), max_depth - 1 ) );
         return *this;
      }
      /**
       * Copy a variant_object into this mutable_variant_object.
       */
      mutable_variant_object& operator()( const variant_object& vo );
      /**
       * Copy another mutable_variant_object into this mutable_variant_object.
       */
      mutable_variant_object& operator()( const mutable_variant_object& mvo );
      ///@}


      template<typename T>
      explicit mutable_variant_object( T& v )
      :_key_value(std::make_unique<std::vector<entry>>() )
      {
          *this = v;
      }

      mutable_variant_object();

      /** initializes the first key/value pair in the object */
      mutable_variant_object( string key, fc::variant val );
      template<typename T>
      mutable_variant_object( string key, T&& val )
      :_key_value( std::make_unique<std::vector<entry> >() )
      {
         set( std::move(key), fc::variant(fc::forward<T>(val)) );
      }

      mutable_variant_object( mutable_variant_object&& );
      mutable_variant_object( const mutable_variant_object& );
      mutable_variant_object( const variant_object& );

      mutable_variant_object& operator=( mutable_variant_object&& );
      mutable_variant_object& operator=( const mutable_variant_object& );
      mutable_variant_object& operator=( const variant_object& );
   private:
      std::unique_ptr< std::vector< entry > > _key_value;
      friend class variant_object;
   };

   class limited_mutable_variant_object : public mutable_variant_object
   {
      public:
         limited_mutable_variant_object( uint32_t max_depth, bool skip_on_exception = false );

         template<typename T>
         limited_mutable_variant_object& operator()( string key, T&& var )
         {
            if( _reached_depth_limit )
               // _skip_on_exception will always be true here
               return *this;

            fc::optional<fc::variant> v;
            try
            {
               v = fc::variant( fc::forward<T>(var), _max_depth );
            }
            catch( ... )
            {
               if( !_skip_on_exception )
                  throw;
               v = fc::variant( "[ERROR: Caught exception while converting data to variant]" );
            }
            set( std::move(key), *v );
            return *this;
         }
         limited_mutable_variant_object& operator()( const variant_object& vo );

      private:
         const uint32_t _max_depth;       ///< The depth limit
         const bool _reached_depth_limit; ///< Indicates whether we've reached depth limit
         const bool _skip_on_exception;   ///< If set to true, won't rethrow exceptions when reached depth limit
   };

   /** @ingroup Serializable */
   void to_variant( const mutable_variant_object& var, fc::variant& vo, uint32_t max_depth = 1 );
   /** @ingroup Serializable */
   void from_variant( const fc::variant& var, mutable_variant_object& vo, uint32_t max_depth = 1 );

} // namespace fc
