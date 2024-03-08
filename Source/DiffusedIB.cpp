
#include <DiffusedIB.H>

#include <AMReX_ParmParse.H>
#include <AMReX_TagBox.H>
#include <AMReX_Utility.H>
#include <AMReX_PhysBCFunct.H>
#include <AMReX_MLNodeLaplacian.H>
#include <AMReX_FillPatchUtil.H>
#include <iamr_constants.H>

using namespace amrex;

void nodal_phi_to_pvf(MultiFab& pvf, const MultiFab& phi_nodal)
{

    Print() << "In the nodal_phi_to_pvf " << std::endl;

#ifdef AMREX_USE_OMP
#pragma omp parallel if (Gpu::notInLaunchRegion())
#endif
    for (MFIter mfi(pvf,TilingIfNotGPU()); mfi.isValid(); ++mfi)
    {
        const Box& bx = mfi.tilebox();
        auto const& pvffab   = pvf.array(mfi);
        auto const& pnfab = phi_nodal.array(mfi);
        amrex::ParallelFor(bx, [pvffab, pnfab]
        AMREX_GPU_DEVICE(int i, int j, int k) noexcept
        {
            Real num = 0.0;
            for(int kk=k; kk<=k+1; kk++) {
                for(int jj=j; jj<=j+1; jj++) {
                    for(int ii=i; ii<=i+1; ii++) {
                        num += (-pnfab(ii,jj,kk)) * nodal_phi_to_heavi(-pnfab(ii,jj,kk));
                    }
                }
            }
            Real deo = 0.0;
            for(int kk=k; kk<=k+1; kk++) {
                for(int jj=j; jj<=j+1; jj++) {
                    for(int ii=i; ii<=i+1; ii++) {
                        deo += std::abs(pnfab(ii,jj,kk));
                    }
                }
            }
            pvffab(i,j,k) = num / (deo + 1.e-12);
        });
    }

}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                     other function                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

[[nodiscard]] AMREX_FORCE_INLINE
Real cal_momentum(Real rho, Real radious)
{
    return 8.0 * Math::pi<Real>() * rho * Math::powi<5>(radious) / 15.0;
}

AMREX_FORCE_INLINE
void deltaFunction(Real xf, Real xp, Real h, Real& value, DELTA_FUNCTION_TYPE type)
{
    Real rr = amrex::Math::abs(( xf - xp ) / h);

    switch (type) {
    case DELTA_FUNCTION_TYPE::FOUR_POINT_IB:
        if(rr >= 0 && rr < 0.5 ){
            value = 1.0 / 8.0 * ( 3.0 - 2.0 * rr + std::sqrt( 1.0 + 4.0 * rr - 4 * Math::powi<2>(rr))) / h;
        }else if (rr >= 1 && rr < 2) {
            value = 1.0 / 8.0 * ( 5.0 - 2.0 * rr - std::sqrt( -7.0 + 12.0 * rr - 4 * Math::powi<2>(rr))) / h;
        }else {
            value = 0;
        }
        break;
    case DELTA_FUNCTION_TYPE::THREE_POINT_IB:
        if(rr >= 0 && rr < 1){
            value = 1.0 / 6.0 * ( 5.0 - 3.0 * rr + std::sqrt( - 3.0 * ( 1 - Math::powi<2>(rr)) + 1.0 )) / h;
        }else if (rr >= 1 && rr < 2) {
            value = 1.0 / 3.0 * ( 1.0 + std::sqrt( 1.0 - 3 * Math::powi<2>(rr))) / h;
        }else {
            value = 0;
        }
        break;
    default:
        break;
    }
}

template <typename P>
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void ForceSpreading_cic (P const& p,
                  ParticleReal fxP,
                  ParticleReal fyP,
                  ParticleReal fzP,
                  Array4<Real> const& E,
                  int EularFIndex,
                  GpuArray<Real,AMREX_SPACEDIM> const& plo,
                  GpuArray<Real,AMREX_SPACEDIM> const& dx,
                  DELTA_FUNCTION_TYPE type)
{
    const Real d = AMREX_D_TERM(dx[0], *dx[1], *dx[2]);
    //plo to ii jj kk
    Real lx = (p.pos(0) - plo[0]) / dx[0];
    Real ly = (p.pos(1) - plo[1]) / dx[1];
    Real lz = (p.pos(2) - plo[2]) / dx[2];

    int i = static_cast<int>(Math::floor(lx));
    int j = static_cast<int>(Math::floor(ly));
    int k = static_cast<int>(Math::floor(lz));
    // calc_delta(i, j, k, dxi, rho);
    //lagrangian to Euler
    for(int ii = -2; ii < +3; ii++){
        for(int jj = -2; jj < +3; jj++){
            for(int kk = -2; kk < +3; kk ++){
                Real tU, tV, tW;
                const Real xi = (i + ii) * dx[0] + dx[0]/2;
                const Real yj = (j + jj) * dx[1] + dx[1]/2;
                const Real kz = (k + kk) * dx[2] + dx[2]/2;
                deltaFunction( p.pos(0), xi, dx[0], tU, type);
                deltaFunction( p.pos(1), yj, dx[1], tV, type);
                deltaFunction( p.pos(2), kz, dx[2], tW, type);
                Real delta_value = tU * tV * tW;
                Gpu::Atomic::AddNoRet(&E(i + ii, j + jj, k + kk, EularFIndex  ), delta_value * fxP * d);
                Gpu::Atomic::AddNoRet(&E(i + ii, j + jj, k + kk, EularFIndex+1), delta_value * fyP * d);
                Gpu::Atomic::AddNoRet(&E(i + ii, j + jj, k + kk, EularFIndex+2), delta_value * fzP * d);
            }
        }
    }
}

template <typename P = Particle<numAttri>>
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void VelocityInterpolation_cir(P const& p, Real& Up, Real& Vp, Real& Wp,
                     Array4<Real const> const& E, int EularVIndex,
                     GpuArray<Real, AMREX_SPACEDIM> const& plo,
                     GpuArray<Real, AMREX_SPACEDIM> const& dx,
                     DELTA_FUNCTION_TYPE type)
{
    const Real d = AMREX_D_TERM(dx[0], *dx[1], *dx[2]);

    const Real lx = (p.pos(0) - plo[0]) / dx[0]; // x
    const Real ly = (p.pos(1) - plo[1]) / dx[1]; // y
    const Real lz = (p.pos(2) - plo[2]) / dx[2]; // z

    int i = static_cast<int>(Math::floor(lx)); // i
    int j = static_cast<int>(Math::floor(ly)); // j
    int k = static_cast<int>(Math::floor(lz)); // k

    Up = 0;
    Vp = 0;
    Wp = 0;
    //Euler to largrangian
    for(int ii = -2; ii < 3; ii++){
        for(int jj = -2; jj < 3; jj++){
            for(int kk = -2; kk < 3; kk ++){
                Real tU, tV, tW;
                const Real xi = (i + ii) * dx[0] + dx[0]/2;
                const Real yj = (j + jj) * dx[1] + dx[1]/2;
                const Real kz = (k + kk) * dx[2] + dx[2]/2;
                deltaFunction( p.pos(0), xi, dx[0], tU, type);
                deltaFunction( p.pos(1), yj, dx[1], tV, type);
                deltaFunction( p.pos(2), kz, dx[2], tW, type);
                const Real delta_value = tU * tV * tW;
                Gpu::Atomic::AddNoRet( &Up, delta_value * E(i + ii, j + jj, k + kk, EularVIndex  ) * d );
                Gpu::Atomic::AddNoRet( &Vp, delta_value * E(i + ii, j + jj, k + kk, EularVIndex+1) * d );
                Gpu::Atomic::AddNoRet( &Wp, delta_value * E(i + ii, j + jj, k + kk, EularVIndex+2) * d );
            }
        }
    }
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                    mParticle member function                  */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
//loop all particels
void mParticle::InteractWithEuler(MultiFab &Euler, int loop_time, Real dt, Real alpha_k, DELTA_FUNCTION_TYPE type){

    for(kernel& kernel : particle_kernels){
        //switch particles
        InitialWithLargrangianPoints(kernel);
        
        UpdateParticles(Euler, kernel, dt, alpha_k);
        const int EulerForceIndex = euler_force_index;
        //for 1 -> Ns
        while(loop_time > 0){
            //clear Euler force
            for(amrex::MFIter mfi(Euler); mfi.isValid(); ++mfi){
                const auto& bx = mfi.validbox();
                const auto& mf_array = Euler.array(mfi);

                amrex::ParallelFor(bx, [mf_array, EulerForceIndex] 
                AMREX_GPU_DEVICE(int i, int j, int k){
                    mf_array(i,j,k,EulerForceIndex  ) = 0.0;//+ std::exp(-r_squared);
                    mf_array(i,j,k,EulerForceIndex+1) = 0.0;
                    mf_array(i,j,k,EulerForceIndex+2) = 0.0;
                });
            }
            //correction
            VelocityInterpolation(Euler, type);
            ComputeLagrangianForce(dt, kernel);
            ForceSpreading(Euler, type);
            //VelocityCorrection
            MultiFab::Saxpy(Euler, dt, Euler, euler_force_index, euler_velocity_index, 3, 0);
            loop_time--;
        };
    }
}

void mParticle::InitParticles(const Vector<Real>& x,
                              const Vector<Real>& y,
                              const Vector<Real>& z,
                              Real rho_s,
                              int radious,
                              Real rho_f, 
                              int force_index, 
                              int velocity_index){                                        
    euler_force_index = force_index;
    euler_fluid_rho = rho_f;
    euler_velocity_index = velocity_index;
    //pre judge
    if(!((x.size() == y.size()) && (x.size() == z.size()))){
        Print() << "particle's position container are all different size";
        return;
    }
    //all the particles have same radious
    Real phiK = 0;
    Real h = m_gdb->Geom(euler_finest_level).CellSizeArray()[0];
    int Ml = static_cast<int>( Math::pi<Real>() / 3 * (12 * Math::powi<2>(radious / h)));
    Real dv = Math::pi<Real>() * h / 3 / Ml * (12 * radious * radious + h * h);

    for(int index = 0; index < x.size(); index++){
        kernel mKernel;
        mKernel.location[0] = x[index];
        mKernel.location[1] = y[index];
        mKernel.location[2] =  z[index];
        mKernel.velocity[0] = 0.0;
        mKernel.velocity[1] = 0.0;
        mKernel.velocity[2] = 0.0;
        mKernel.omega[0] = 0.0;
        mKernel.omega[1] = 0.0;
        mKernel.omega[2] = 0.0;
        mKernel.varphi[0] = 0.0;
        mKernel.varphi[1] = 0.0;
        mKernel.varphi[2] = 0.0;
        mKernel.radious = radious;
        mKernel.ml = Ml;
        mKernel.dv = dv;
        mKernel.rho = rho_s;
        particle_kernels.push_back(mKernel);
    }

    //get particle tile
    std::pair<int, int> key{0,0};
    auto& particleTileTmp = GetParticles(0)[key];
    //insert markers
    if ( ParallelDescriptor::MyProc() == ParallelDescriptor::IOProcessorNumber() ) {
        //insert particle's markers
        for(int marker_index = 0; marker_index < Ml; marker_index++){
            //insert code
            ParticleType markerP;
            markerP.id() = ParticleType::NextID();
            markerP.cpu() = ParallelDescriptor::MyProc();
            markerP.pos(0) = 0;
            markerP.pos(1) = 0;
            markerP.pos(2) = 0;

            std::array<ParticleReal, numAttri> Marker_attr;
            Marker_attr[U_Marker] = 0.0;
            Marker_attr[V_Marker] = 0.0;
            Marker_attr[W_Marker] = 0.0;
            Marker_attr[Fx_Marker] = 0.0;
            Marker_attr[Fy_Marker] = 0.0;
            Marker_attr[Fz_Marker] = 0.0;
            // attr[V] = 10.0;
            particleTileTmp.push_back(markerP);
            particleTileTmp.push_back_real(Marker_attr);
        }
    }
    Redistribute();
}

void mParticle::InitialWithLargrangianPoints(const kernel& current_kernel){
    mParIter pti(*this, euler_finest_level);
    auto *particles = pti.GetArrayOfStructs().data();

    Real phiK = 0;
    for(int index = 0; index < current_kernel.ml; index++){
        Real Hk = -1.0 + 2.0 * (index) / ( current_kernel.ml - 1.0);
        Real thetaK = std::acos(Hk);
        if(index == 0 || index == ( current_kernel.ml - 1)){
            phiK = 0;
        }else {
            phiK = std::fmod( phiK + 3.809 / std::sqrt(current_kernel.ml) / std::sqrt( 1 - Math::powi<2>(Hk)) , 2 * Math::pi<Real>());
        }
        // update LargrangianPoint position with particle position           
        particles[index].pos(0) = current_kernel.location[0] + current_kernel.radious * std::sin(thetaK) * std::cos(phiK);
        particles[index].pos(1) = current_kernel.location[1] + current_kernel.radious * std::sin(thetaK) * std::sin(phiK);
        particles[index].pos(2) = current_kernel.location[2] + current_kernel.radious * std::cos(thetaK);
    }
}

void mParticle::VelocityInterpolation(const MultiFab &Euler,
                                      DELTA_FUNCTION_TYPE type)//
{
    const auto& gm = m_gdb->Geom(euler_finest_level);
    auto plo = gm.ProbLoArray();
    auto dx = gm.CellSizeArray();
    const int EulerVelocityIndex = euler_velocity_index;
    //assert
    //AMREX_ASSERT(OnSameGrids(euler_finest_level, *Euler[0]));

    for(mParIter pti(*this, euler_finest_level); pti.isValid(); ++pti){
        auto& particles = pti.GetArrayOfStructs();
        auto *p_ptr = particles.data();
        const Long np = pti.numParticles();

        auto& attri = pti.GetAttribs();
        auto* Up = attri[P_ATTR::U_Marker].data();
        auto* Vp = attri[P_ATTR::V_Marker].data();
        auto* Wp = attri[P_ATTR::W_Marker].data();
        const auto& E = Euler[pti].array();

        amrex::ParallelFor(np, [=] 
        AMREX_GPU_DEVICE (int i) noexcept{
            VelocityInterpolation_cir(p_ptr[i], Up[i], Vp[i], Wp[i], E, EulerVelocityIndex, plo, dx, type);
        });
    }
    WriteAsciiFile(amrex::Concatenate("particle", 1));
}

void mParticle::ForceSpreading(MultiFab & Euler, 
                               DELTA_FUNCTION_TYPE type){
    int index = 0;
    const auto& gm = m_gdb->Geom(euler_finest_level);
    auto plo = gm.ProbLoArray();
    auto dxi = gm.CellSizeArray();
    const int EulerForceIndex = euler_force_index;
    for(mParIter pti(*this, euler_finest_level); pti.isValid(); ++pti){
        const Long np = pti.numParticles();
        const auto& fxP = pti.GetStructOfArrays().GetRealData(P_ATTR::U_Marker);//Fx_Marker 
        const auto& fyP = pti.GetStructOfArrays().GetRealData(P_ATTR::V_Marker);//Fy_Marker 
        const auto& fzP = pti.GetStructOfArrays().GetRealData(P_ATTR::W_Marker);//Fz_Marker 
        const auto& particles = pti.GetArrayOfStructs();

        auto Uarray = Euler[pti].array();

        const auto& fxP_ptr = fxP.data();
        const auto& fyP_ptr = fyP.data();
        const auto& fzP_ptr = fzP.data();
        const auto& p_ptr = particles().data();
        amrex::ParallelFor(np, [=] 
        AMREX_GPU_DEVICE (int i) noexcept{
            ForceSpreading_cic(p_ptr[i], fxP_ptr[i], fyP_ptr[i], fzP_ptr[i], Uarray, EulerForceIndex, plo, dxi, type);
        });
    }
}

void mParticle::UpdateParticles(const amrex::MultiFab& Euler, kernel& kernel, Real dt, Real alpha_k)
{
    const auto& gm = m_gdb->Geom(euler_finest_level);
    auto plo = gm.ProbLoArray();
    auto dxi = gm.InvCellSizeArray();
    //update the kernel's infomation and cal body force
    for(mParIter pti(*this, euler_finest_level); pti.isValid(); ++pti){
        auto &particles = pti.GetArrayOfStructs();
        auto *p_ptr = particles.data();
        auto &attri = pti.GetAttribs();
        auto *FxP = attri[P_ATTR::Fx_Marker].data();
        auto *FyP = attri[P_ATTR::Fy_Marker].data();
        auto *FzP = attri[P_ATTR::Fz_Marker].data();
        auto *UP  = attri[P_ATTR::U_Marker].data();
        auto *VP  = attri[P_ATTR::V_Marker].data();
        auto *WP  = attri[P_ATTR::W_Marker].data();
        const Real Dv = kernel.dv;
        const Long np = pti.numParticles();
        RealVect ForceDv{std::vector<Real>{0.0,0.0,0.0}};
        RealVect Moment{std::vector<Real>{0.0,0.0,0.0}};
        auto *ForceDv_ptr  = &ForceDv;
        auto *Moment_ptr   = &Moment;
        auto *location_ptr = &kernel.location;
        auto *omega_ptr    = &kernel.omega;
        auto *velocity_ptr = &kernel.velocity;
        auto *varphi_ptr   = &kernel.varphi;
        const Real rho_p = kernel.rho;
        //sum
        amrex::ParallelFor(np, [=] 
        AMREX_GPU_DEVICE (int i) noexcept{
            //calculate the force
            //find current particle's lagrangian marker
            *ForceDv_ptr += RealVect(AMREX_D_DECL(FxP[i],FyP[i],FzP[i])) * Dv;
            *Moment_ptr +=  (RealVect(AMREX_D_DECL(p_ptr[i].pos(0),p_ptr[i].pos(1),p_ptr[i].pos(2))) - *location_ptr).crossProduct(
                            RealVect(AMREX_D_DECL(FxP[i],FyP[i],FzP[i]))) * Dv;
        });
        RealVect oldVelocity = kernel.velocity;
        RealVect oldOmega = kernel.omega;
        kernel.velocity = kernel.velocity -
                            2 * alpha_k * dt/( Math::pi<Real>() * 4 * Math::powi<3>(kernel.radious) / 3) / (kernel.rho - euler_fluid_rho) * (ForceDv);// + mVector(0.0, -9.8, 0.0));
        kernel.omega = kernel.omega -
                        2 * alpha_k * dt * kernel.rho / cal_momentum(kernel.rho, kernel.radious) / (kernel.rho - euler_fluid_rho) * Moment;
        
        auto deltaX = alpha_k * dt * (*velocity_ptr + oldVelocity);
        *location_ptr = *location_ptr + deltaX;
        *varphi_ptr = *varphi_ptr + alpha_k * dt * (*omega_ptr + oldOmega);
        //sum
        auto Uarray = Euler[pti].array();
        amrex::ParallelFor(np, [=] 
        AMREX_GPU_DEVICE (int i) noexcept{
            //calculate the force
            //find current particle's lagrangian marker
            RealVect tmp = (*omega_ptr).crossProduct(*location_ptr - RealVect(p_ptr[i].pos(0),
                                                                                    p_ptr[i].pos(1),
                                                                                    p_ptr[i].pos(2)));
            FxP[i] = rho_p / dt *(UP[i] + tmp[0]);
            FyP[i] = rho_p / dt *(VP[i] + tmp[1]);
            FzP[i] = rho_p / dt *(WP[i] + tmp[2]);
            p_ptr[i].pos(0) += deltaX[0];
            p_ptr[i].pos(1) += deltaX[1];
            p_ptr[i].pos(2) += deltaX[2];
        });
    }
}

void mParticle::ComputeLagrangianForce(Real dt, const kernel& kernel)
{
    Real Ub = kernel.velocity[0];
    Real Vb = kernel.velocity[1];
    Real Wb = kernel.velocity[2];

    for(mParIter pti(*this, euler_finest_level); pti.isValid(); ++pti){
        auto& particles = pti.GetArrayOfStructs();
        auto *p_ptr = particles.data();
        const Long np = pti.numParticles();

        auto& attri = pti.GetAttribs();
        auto* Up = attri[P_ATTR::U_Marker].data();
        auto* Vp = attri[P_ATTR::V_Marker].data();
        auto* Wp = attri[P_ATTR::W_Marker].data();
        auto *FxP = attri[P_ATTR::Fx_Marker].data();
        auto *FyP = attri[P_ATTR::Fy_Marker].data();
        auto *FzP = attri[P_ATTR::Fz_Marker].data();
        amrex::ParallelFor(np,
        [=] AMREX_GPU_DEVICE (int i) noexcept{
            FxP[i] = (Ub - Up[i])/dt; //
            FyP[i] = (Vb - Vp[i])/dt; //
            FzP[i] = (Wb - Wp[i])/dt; //
        });
    }
}

void mParticle::WriteParticleFile(int index)
{
    WriteAsciiFile(amrex::Concatenate("particle", index));
}
