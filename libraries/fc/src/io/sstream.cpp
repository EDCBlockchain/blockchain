#include <fc/io/sstream.hpp>
#include <fc/fwd_impl.hpp>
#include <fc/exception/exception.hpp>
#include <fc/log/logger.hpp>
#include <sstream>

namespace fc {
  class stringstream::impl {
    public:
    impl( std::string&s )
    :ss( s )
    { ss.exceptions( std::stringstream::badbit ); }

    impl( const std::string&s )
    :ss( s )
    { ss.exceptions( std::stringstream::badbit ); }

    impl(){ss.exceptions( std::stringstream::badbit ); }
    
    std::stringstream ss;
  };

  stringstream::stringstream( std::string& s )
  :my(s) {
  }
  stringstream::stringstream( const std::string& s )
  :my(s) {
  }
  stringstream::stringstream(){}
  stringstream::~stringstream(){}


  std::string stringstream::str(){
    return my->ss.str();//.c_str();//*reinterpret_cast<std::string*>(&st);
  }

  void stringstream::str(const std::string& s) {
    my->ss.str(s);
  }

  void stringstream::clear() {
    my->ss.clear();
  }


  bool     stringstream::eof()const {
    return my->ss.eof();
  }
  size_t stringstream::writesome( const char* buf, size_t len ) {
    my->ss.write(buf,len);
    if( my->ss.eof() )
    {
       FC_THROW_EXCEPTION( eof_exception, "stringstream" );
    }
    return len;
  }
  size_t stringstream::writesome( const std::shared_ptr<const char>& buf, size_t len, size_t offset )
  {
    return writesome(buf.get() + offset, len);
  }

  size_t   stringstream::readsome( char* buf, size_t len ) {
    size_t r = static_cast<size_t>(my->ss.readsome(buf,len));
    if( my->ss.eof() || r == 0 )
    {
       FC_THROW_EXCEPTION( eof_exception, "stringstream" );
    }
    return r;
  }
  size_t   stringstream::readsome( const std::shared_ptr<char>& buf, size_t len, size_t offset )
  {
    return readsome(buf.get() + offset, len);
  }


  void     stringstream::close(){ my->ss.flush(); };
  void     stringstream::flush(){ my->ss.flush(); };

  char     stringstream::peek() 
  { 
    char c = my->ss.peek(); 
    if( my->ss.eof() )
    {
       FC_THROW_EXCEPTION( eof_exception, "stringstream" );
    }
    return c;
  }
} 


