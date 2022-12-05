//
// Copyright (c) 2016-2022 CNRS INRIA
//

#ifndef __eigenpy_utils_std_vector_hpp__
#define __eigenpy_utils_std_vector_hpp__

#include <boost/mpl/if.hpp>
#include <boost/python.hpp>
#include <boost/python/stl_iterator.hpp>
#include <boost/python/suite/indexing/vector_indexing_suite.hpp>
#include <iterator>
#include <string>
#include <vector>

#include "eigenpy/config.hpp"
#include "eigenpy/copyable.hpp"
#include "eigenpy/pickle-vector.hpp"
#include "eigenpy/registration.hpp"

namespace eigenpy {
// Forward declaration
template <typename vector_type, bool NoProxy = false>
struct StdContainerFromPythonList;

namespace details {

/// \brief Check if a PyObject can be converted to an std::vector<T>.
template <typename T>
bool from_python_list(PyObject *obj_ptr, T *) {
  namespace bp = ::boost::python;

  // Check if it is a list
  if (!PyList_Check(obj_ptr)) return false;

  // Retrieve the underlying list
  bp::object bp_obj(bp::handle<>(bp::borrowed(obj_ptr)));
  bp::list bp_list(bp_obj);
  bp::ssize_t list_size = bp::len(bp_list);

  // Check if all the elements contained in the current vector is of type T
  for (bp::ssize_t k = 0; k < list_size; ++k) {
    bp::extract<T> elt(bp_list[k]);
    if (!elt.check()) return false;
  }

  return true;
}

template <typename vector_type, bool NoProxy>
struct build_list {
  static ::boost::python::list run(vector_type &vec) {
    namespace bp = ::boost::python;

    bp::list bp_list;
    for (size_t k = 0; k < vec.size(); ++k) {
      bp_list.append(boost::ref(vec[k]));
    }
    return bp_list;
  }
};

template <typename vector_type>
struct build_list<vector_type, true> {
  static ::boost::python::list run(vector_type &vec) {
    namespace bp = ::boost::python;

    typedef bp::iterator<vector_type> iterator;
    return bp::list(iterator()(vec));
  }
};
}  // namespace details
}  // namespace eigenpy

namespace boost {
namespace python {
namespace converter {

template <typename Type, class Allocator>
struct reference_arg_from_python<std::vector<Type, Allocator> &>
    : arg_lvalue_from_python_base {
  typedef std::vector<Type, Allocator> vector_type;
  typedef vector_type &ref_vector_type;
  typedef ref_vector_type result_type;

  reference_arg_from_python(PyObject *py_obj)
      : arg_lvalue_from_python_base(converter::get_lvalue_from_python(
            py_obj, registered<vector_type>::converters)),
        m_data(NULL),
        m_source(py_obj),
        vec_ptr(NULL) {
    if (result() != 0)  // we have found a lvalue converter
      return;

    // Check if py_obj is a py_list, which can then be converted to an
    // std::vector
    bool is_convertible =
        ::eigenpy::details::from_python_list(py_obj, (Type *)(0));
    if (!is_convertible) return;

    typedef ::eigenpy::StdContainerFromPythonList<vector_type> Constructor;
    Constructor::construct(py_obj, &m_data.stage1);

    void *&m_result = const_cast<void *&>(result());
    m_result = m_data.stage1.convertible;
    vec_ptr = reinterpret_cast<vector_type *>(m_data.storage.bytes);
  }

  result_type operator()() const {
    return ::boost::python::detail::void_ptr_to_reference(result(),
                                                          (result_type(*)())0);
  }

  ~reference_arg_from_python() {
    if (m_data.stage1.convertible == m_data.storage.bytes) {
      // Copy back the reference
      const vector_type &vec = *vec_ptr;
      list bp_list(handle<>(borrowed(m_source)));
      for (size_t i = 0; i < vec.size(); ++i) {
        Type &elt = extract<Type &>(bp_list[i]);
        elt = vec[i];
      }
    }
  }

 private:
  rvalue_from_python_data<ref_vector_type> m_data;
  PyObject *m_source;
  vector_type *vec_ptr;
};

}  // namespace converter
}  // namespace python
}  // namespace boost

namespace eigenpy {

///
/// \brief Register the conversion from a Python list to a std::vector
///
/// \tparam vector_type A std container (e.g. std::vector or std::list)
///
template <typename vector_type, bool NoProxy>
struct StdContainerFromPythonList {
  typedef typename vector_type::value_type T;
  typedef typename vector_type::allocator_type Allocator;

  /// \brief Check if obj_ptr can be converted
  static void *convertible(PyObject *obj_ptr) {
    namespace bp = boost::python;

    // Check if it is a list
    if (!PyList_Check(obj_ptr)) return 0;

    // Retrieve the underlying list
    bp::object bp_obj(bp::handle<>(bp::borrowed(obj_ptr)));
    bp::list bp_list(bp_obj);
    bp::ssize_t list_size = bp::len(bp_list);

    // Check if all the elements contained in the current vector is of type T
    for (bp::ssize_t k = 0; k < list_size; ++k) {
      bp::extract<T> elt(bp_list[k]);
      if (!elt.check()) return 0;
    }

    return obj_ptr;
  }

  /// \brief Allocate the std::vector and fill it with the element contained in
  /// the list
  static void construct(
      PyObject *obj_ptr,
      boost::python::converter::rvalue_from_python_stage1_data *memory) {
    namespace bp = boost::python;

    // Extract the list
    bp::object bp_obj(bp::handle<>(bp::borrowed(obj_ptr)));
    bp::list bp_list(bp_obj);

    void *storage =
        reinterpret_cast<
            bp::converter::rvalue_from_python_storage<vector_type> *>(
            reinterpret_cast<void *>(memory))
            ->storage.bytes;

    typedef bp::stl_input_iterator<T> iterator;

    // Build the std::vector
    new (storage) vector_type(iterator(bp_list), iterator());

    // Validate the construction
    memory->convertible = storage;
  }

  static void register_converter() {
    ::boost::python::converter::registry::push_back(
        &convertible, &construct, ::boost::python::type_id<vector_type>());
  }

  static ::boost::python::list tolist(vector_type &self) {
    return details::build_list<vector_type, NoProxy>::run(self);
  }
};

namespace internal {

template <typename T>
struct has_operator_equal
    : boost::mpl::if_<typename boost::is_base_of<Eigen::EigenBase<T>, T>::type,
                      has_operator_equal<Eigen::EigenBase<T> >,
                      boost::true_type>::type {};

template <typename T, class A>
struct has_operator_equal<std::vector<T, A> > : has_operator_equal<T> {};

template <>
struct has_operator_equal<bool> : boost::true_type {};

template <typename EigenObject>
struct has_operator_equal<Eigen::EigenBase<EigenObject> >
    : has_operator_equal<typename EigenObject::Scalar> {};

template <typename T, bool has_operator_equal_value = boost::is_base_of<
                          boost::true_type, has_operator_equal<T> >::value>
struct contains_algo;

template <typename T>
struct contains_algo<T, true> {
  template <class Container, typename key_type>
  static bool run(Container &container, key_type const &key) {
    return std::find(container.begin(), container.end(), key) !=
           container.end();
  }
};

template <typename T>
struct contains_algo<T, false> {
  template <class Container, typename key_type>
  static bool run(Container &container, key_type const &key) {
    for (size_t k = 0; k < container.size(); ++k) {
      if (&container[k] == &key) return true;
    }
    return false;
  }
};

template <class Container, bool NoProxy>
struct contains_vector_derived_policies
    : public ::boost::python::vector_indexing_suite<
          Container, NoProxy,
          contains_vector_derived_policies<Container, NoProxy> > {
  typedef typename Container::value_type key_type;

  static bool contains(Container &container, key_type const &key) {
    return contains_algo<key_type>::run(container, key);
  }
};
}  // namespace internal

struct EmptyPythonVisitor
    : public ::boost::python::def_visitor<EmptyPythonVisitor> {
  template <class classT>
  void visit(classT &) const {}
};

///
/// \brief Expose an std::vector from a type given as template argument.
///
/// \tparam T Type to expose as std::vector<T>.
/// \tparam Allocator Type for the Allocator in std::vector<T,Allocator>.
/// \tparam NoProxy When set to false, the elements will be copied when returned
/// to Python. \tparam EnableFromPythonListConverter Enables the conversion from
/// a Python list to a std::vector<T,Allocator>
///
/// \sa StdAlignedVectorPythonVisitor
///
template <class vector_type, bool NoProxy = false,
          bool EnableFromPythonListConverter = true>
struct StdVectorPythonVisitor
    : public ::boost::python::vector_indexing_suite<
          vector_type, NoProxy,
          internal::contains_vector_derived_policies<vector_type, NoProxy> >,
      public StdContainerFromPythonList<vector_type, NoProxy> {
  typedef typename vector_type::value_type value_type;
  typedef typename vector_type::allocator_type allocator_type;
  typedef StdContainerFromPythonList<vector_type, NoProxy>
      FromPythonListConverter;

  static void expose(const std::string &class_name,
                     const std::string &doc_string = "") {
    expose(class_name, doc_string, EmptyPythonVisitor());
  }

  template <typename VisitorDerived>
  static void expose(
      const std::string &class_name,
      const boost::python::def_visitor<VisitorDerived> &visitor) {
    expose(class_name, "", visitor);
  }

  template <typename VisitorDerived>
  static void expose(
      const std::string &class_name, const std::string &doc_string,
      const boost::python::def_visitor<VisitorDerived> &visitor) {
    namespace bp = boost::python;

    if (!register_symbolic_link_to_registered_type<vector_type>()) {
      bp::class_<vector_type> cl(class_name.c_str(), doc_string.c_str());
      cl.def(StdVectorPythonVisitor())

          .def(bp::init<size_t, const value_type &>(
              bp::args("self", "size", "value"),
              "Constructor from a given size and a given value."))
          .def(bp::init<const vector_type &>(bp::args("self", "other"),
                                             "Copy constructor"))

          .def("tolist", &FromPythonListConverter::tolist, bp::arg("self"),
               "Returns the std::vector as a Python list.")
          .def(visitor)
          .def("reserve", &vector_type::reserve,
               (bp::arg("self"), bp::arg("new_cap")),
               "Increase the capacity of the vector to a value that's greater "
               "or equal to new_cap.")
          .def_pickle(PickleVector<vector_type>())
          .def(CopyableVisitor<vector_type>());

      // Register conversion
      if (EnableFromPythonListConverter)
        FromPythonListConverter::register_converter();
    }
  }
};

/**
 * Expose std::vector for given matrix or vector sizes.
 */
void EIGENPY_DLLAPI exposeStdVector();

}  // namespace eigenpy

#endif  // ifndef __eigenpy_utils_std_vector_hpp__
