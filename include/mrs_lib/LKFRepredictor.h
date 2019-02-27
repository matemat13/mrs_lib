#ifndef LKFREPREDICTOR_H
#define LKFREPREDICTOR_H

#include "mrs_lib/Repredictor.h"

namespace mrs_lib
{

  /* class Model_lkf //{ */
  template <int n_states, int n_inputs, int n_measurements>
  class Model_lkf : public Repredictor<n_states, n_inputs, n_measurements>::Model
  {
    public:
    /* LKF definitions (typedefs, constants etc) //{ */
    static const unsigned n = n_states;
    static const unsigned m = n_inputs;
    static const unsigned p = n_measurements;
    using Repr = Repredictor<n, m, p>;

    using x_t = typename Repr::x_t;  // state vector n*1
    using u_t = typename Repr::u_t;  // input vector m*1
    using z_t = typename Repr::z_t;  // measurement vector p*1
    using P_t = typename Repr::P_t;  // state covariance n*n
    using R_t = typename Repr::R_t;  // measurement covariance p*p
    using statecov_t = typename Repr::statecov_t;  // helper struct for state and covariance

    typedef Eigen::Matrix<double, n, n> A_t;  // system matrix n*n
    typedef Eigen::Matrix<double, n, m> B_t;  // input matrix n*m
    typedef Eigen::Matrix<double, p, n> H_t;  // measurement mapping p*n
    typedef Eigen::Matrix<double, n, n> Q_t;  // process covariance n*n
    typedef Eigen::Matrix<double, n, p> K_t;  // kalman gain n*p

    struct inverse_exception : public std::exception
    {
      const char *what() const throw() {
        return "LKF: could not compute matrix inversion!!! Fix your covariances (the measurement's is probably too low...)";
      }
    };
    //}

    public:
      Model_lkf()
      {};

      Model_lkf(const A_t& A, const B_t& B, const H_t& H, const Q_t& Q)
        : A(A), B(B), H(H), Q(Q)
      {};

      /* correct() method //{ */
      virtual statecov_t correct(const statecov_t& sc, const z_t& z, const R_t& R) const
      {
        return correction_impl(sc, z, R, H);
      };
      //}

      /* predict() method //{ */
      virtual statecov_t predict(const statecov_t& sc, const u_t& u, [[maybe_unused]] double dt) const
      {
        statecov_t ret;
        ret.x = state_predict(A, sc.x, B, u);
        ret.P = covariance_predict(A, sc.P, Q);
      };
      //}

    public:
      A_t A;  // system matrix n*n
      B_t B;  // input matrix n*m
      H_t H;  // measurement mapping p*n
      Q_t Q;  // process covariance n*n

    public:
      /* covariance_predict() method //{ */
      static P_t covariance_predict(const A_t& A, const P_t& P, const Q_t& Q)
      {
        return A*P*A.transpose() + Q;
      }
      //}

      /* state_predict() method //{ */
      template<bool check=n_inputs>
      static inline
      typename std::enable_if<check==0, x_t>::type
      state_predict(const A_t& A, const x_t& x, [[maybe_unused]] const B_t& B, [[maybe_unused]] const u_t& u)
      {
        return A*x;
      }

      template<bool check=n_inputs>
      static inline
      typename std::enable_if<check!=0, x_t>::type
      state_predict(const A_t& A, const x_t& x, const B_t& B, const u_t& u)
      {
        return A*x + B*u;
      }
      //}
      
    protected:
      /* correction_impl() method //{ */
      static statecov_t correction_impl(const statecov_t& sc, const z_t& z, const R_t& R, const H_t& H)
      {
        statecov_t ret;
        // the correction phase
        R_t tmp = H*sc.P*H.transpose() + R;
      
        Eigen::ColPivHouseholderQR<R_t> qr(tmp);
        if (!qr.isInvertible())
        {
          // add some stuff to the tmp matrix diagonal to make it invertible
          R_t ident = R_t::Identity(R.rows(), R.cols());
          tmp += 1e-9*ident;
          qr.compute(tmp);
          if (!qr.isInvertible())
          {
            // never managed to make this happen except for explicitly putting NaNs in the input
            throw inverse_exception();
          }
        }
        tmp = qr.inverse();
      
        const K_t K = sc.P*H.transpose()*tmp;
        ret.x           = sc.x + K * (z - (H*sc.x));
        ret.P           = (P_t::Identity() - (K*H))*sc.P;
        return ret;
      }
      //}

  };
  //}

  /* class Model_mass_point //{ */
  template <int n_states, int n_inputs, int n_measurements>
  class Model_lkf_dt : public Model_lkf<n_states, n_inputs, n_measurements>
  {
    public:
    /* LKF definitions (typedefs, constants etc) //{ */
    static const unsigned n = n_states;
    static const unsigned m = n_inputs;
    static const unsigned p = n_measurements;
    using Base_class = Model_lkf<n, m, p>;

    using x_t = typename Base_class::x_t;
    using u_t = typename Base_class::u_t;
    using z_t = typename Base_class::z_t;
    using P_t = typename Base_class::P_t;
    using R_t = typename Base_class::R_t;
    using statecov_t = typename Base_class::statecov_t;
    using A_t = typename Base_class::A_t;  // measurement mapping p*n
    using B_t = typename Base_class::B_t;  // process covariance n*n
    using H_t = typename Base_class::H_t;  // measurement mapping p*n
    using Q_t = typename Base_class::Q_t;  // process covariance n*n

    using coeff_A_t = A_t;  // matrix of constant coefficients in matrix A
    typedef Eigen::Matrix<unsigned, n, n> dtexp_A_t;  // matrix of dt exponents in matrix A
    using coeff_B_t = B_t;  // matrix of constant coefficients in matrix B
    typedef Eigen::Matrix<unsigned, n, m> dtexp_B_t;  // matrix of dt exponents in matrix B
    //}

    public:
      Model_lkf_dt(const H_t& H, const Q_t& Q, const coeff_A_t& coeff_A, const dtexp_B_t& dtexp_A, const coeff_B_t& coeff_B, const dtexp_A_t& dtexp_B)
        : coeff_A(coeff_A), dtexp_A(dtexp_A), coeff_B(coeff_B), dtexp_B(dtexp_B)
      {
        Base_class::H = H;
        Base_class::Q = Q;
      };

      /* predict() method //{ */
      virtual statecov_t predict(const statecov_t& sc, const u_t& u, double dt) const
      {
        statecov_t ret;
        A_t A = get_A(coeff_A, dtexp_A, dt);
        B_t B = get_B(coeff_A, dtexp_B, dt);
        ret.x = Base_class::state_predict(A, sc.x, B, u);
        ret.P = Base_class::covariance_predict(A, sc.P, Base_class::Q);
        return ret;
      };
      //}

    public:
      coeff_A_t coeff_A;
      dtexp_A_t dtexp_A;
      coeff_B_t coeff_B;
      dtexp_B_t dtexp_B;

    public:
      /* get_A() and get_B() methods //{ */
      static A_t get_A(const coeff_A_t& coeff_A, const dtexp_A_t& dtexp_A, double dt)
      {
        A_t ret = A_t::Constant(dt);
        for (int i = 0; i < n; i++)
        {
          for (int j = 0; j < n; j++)
          {
            int exp = dtexp_A(i, j);
            for (int p = 0; p < exp; p++)
            {
              ret(i, j) *= ret(i, j);
            }
            ret(i, j) *= coeff_A(i, j);
          }
        }
        return ret;
      };
      static B_t get_B(const coeff_B_t& coeff_B, const dtexp_B_t& dtexp_B, double dt)
      {
        B_t ret = B_t::Constant(dt);
        for (int i = 0; i < n; i++)
        {
          for (int j = 0; j < n; j++)
          {
            int exp = dtexp_B(i, j);
            for (int p = 0; p < exp; p++)
            {
              ret(i, j) *= ret(i, j);
            }
            ret(i, j) *= coeff_B(i, j);
          }
        }
        return ret;
      };
      //}

  };
  //}

}  // namespace uav_localize

#endif // LKFREPREDICTOR_H
