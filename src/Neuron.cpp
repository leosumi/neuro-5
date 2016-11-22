#include "Neuron.hpp"
#include <cmath>
#include <string>
#include <assert.h>
#include <chrono>
#include <random>
#include "Simulation.hpp"

unsigned int Neuron::neuron_id_ = 0;
Physics::Potential const Neuron::firing_threshold_= FIRING_THRESHOLD;
Physics::Time const Neuron::refractory_period_ = REFRACTORY_PERIOD;
Physics::Potential const Neuron::resting_potential_= RESTING_POTENTIAL;
Physics::Potential const Neuron::reset_potential_= RESET_POTENTIAL;
Physics::Time const Neuron::transmission_delay_= TRANSMISSION_DELAY;
Physics::Resistance const Neuron::membrane_resistance_ = MEMBRANE_RESISTANCE;
Physics::Time const Neuron::tau_ = TAU;

using namespace std;

Neuron::Neuron(Type const& a_type, bool const& exc, double const& external_factor)
                : type_(a_type), excitatory_(exc), external_factor_(external_factor), t_(0)
{
    string fileName =  "neuron_" + to_string(neuron_id_) + ".csv";
    neuron_file = new ofstream(fileName);
    Vm_ = resting_potential_;
    last_spike_time_ = -Neuron::refractory_period_;
    //no spike is added between last_spike_time_ and last_spike_time_+refractory_period
    //see add_event_in() function (discards spikes during refraction)

    J_ = exc ? WEIGHT_EXC : WEIGHT_INH; //if (exc) J_=WEIGHT_EXC; else J_=WEIGHT_INH;


    if (neuron_file->fail()) {
        throw string("Error: The file doesn't exist !");
    } else {
        *neuron_file << "t [ms]" << "," << "Vm [V]" << endl;
    }
    ++neuron_id_;
}


Neuron::~Neuron()
{  
    neuron_file->close();
    delete neuron_file;
}


void Neuron::input(Physics::Time const& dt)
{
	if(type_ == Type::Analytic)
	{
		double dt_as = dt+transmission_delay_;
		update_RI(dt_as);
		step(dt_as);
	}
	else
	{
		update_RI(dt);
		step(dt);
    }
}


void Neuron::output(double const& x)
{

    Event ev(t_+transmission_delay_, x);

    for (size_t i = 0; i < synapses_.size(); ++i)
    {
        assert(synapses_[i]!=NULL);
        synapses_[i]->add_event_in(ev);
    }
}


bool Neuron::has_reached_threshold() const
{
    return Vm_ >= firing_threshold_;
}


void Neuron::add_event_in(Event const& ev)
{
    //only adds spikes that are delivered after refraction
    if (ev.get_t() >= last_spike_time_ + refractory_period_)
        events_in_.push(ev);
}


// remet le potentiel de membrane au potentiel au repos, cette méthode sera appelée
// dans update si le threshold est dépassé
void Neuron::reset_potential()
{
    Vm_ = reset_potential_;
}


double Neuron::external_spike_generator(Physics::Time const& dt)
{
	/// Construct a random generator engine from a time-based seed
	auto seed = std::chrono::system_clock::now().time_since_epoch().count();
	std::default_random_engine generator(seed);

	/// Use a Poisson distribution with a rate 
	//std::poisson_distribution<double> distribution(external_factor_*dt);
	//return distribution(generator);
	return 0;		
}


void Neuron::update(Physics::Time const& dt)
{
    input(dt); //<met d'abord à jour les input (ce que le neurone reçoit)
    //<décrémenter refractory period jusqu'à 0 pas en dessous
    //< output à toutes ses connexions dans le cas où le threshold est atteint
    //< et le courant est remis à 0
    //< output à toutes ses connexions dans le cas où le threshold est atteint
    //< et le courant est remis à 0

    if (has_reached_threshold())
    {
        output(J_);
        *neuron_file << t_ << "," << Vm_ << endl;
        last_spike_time_ = t_;
        reset_potential();

        if (type_ == Type::Analytic)
        {
            t_ += refractory_period_;
            *neuron_file << t_ << "," << Vm_ << endl;
        }
    }
   
    *neuron_file << t_ << "," << Vm_ << endl;
    
}


void Neuron::set_connection(Neuron* neuron)
{
    assert(neuron!=NULL);
	synapses_.push_back(neuron);
}

void Neuron::step(Physics::Time const& dt) // faire en sorte que dans commandline on puisse entrer que 0,1,2
{
	switch(type_)
	{
		case Type::Analytic :
		     step_analytic(dt);
		     break;

		case Type::Explicit :
		     step_explicit(dt);
		     break;

		case Type::Implicit :
		     step_implicit(dt);
		     break;
	}
	t_+=dt;
}


void Neuron::step_analytic(Physics::Time const& dt)
{
	for (int i=1; i<4; i++)
    {
		double temp_Vm = Vm_ * exp((-dt/4*i)/tau_);
     	*neuron_file << this->t_ + dt/4*i  << "," << temp_Vm << endl;
    }
	Vm_ *= exp(-dt/tau_); //new voltage from voltage decay from previous step
    Vm_ += membrane_resistance_ * J_; //network current
}


void Neuron::step_explicit(Physics::Time const& dt)// Use of V(t-1)=Vm_ to calculate the new Vm_
{
    Vm_ += ((-Vm_ + membrane_resistance_ * J_) * dt) / tau_;
}


void Neuron::step_implicit(Physics::Time const& dt)
{
    Vm_ = ((dt * membrane_resistance_ * J_ ) + (tau_ * Vm_)) / ( dt + tau_);
}


void Neuron::update_RI(Physics::Time const& dt)
{
    J_ = 0;
	if(type_ == Type::Analytic)
	{
        while( !events_in_.empty()
            && Physics::dirac_distribution(t_- transmission_delay_ - events_in_.top().get_t()) == 1 )
		{
            J_ += tau_/membrane_resistance_* events_in_.top().get_J();
			events_in_.pop();
		}
	}

	else if ((type_ == Type::Explicit) or (type_ == Type::Implicit))
	{
        while((!events_in_.empty())
           && (events_in_.top().get_t() < t_ + dt))
		{
		   
		// si la différence entre le temps courant et (transmission_delay_ + temps où le courant a été envoyé) = 0
		// la fonction de dirac retourne 1 et dans ce cas on incrémente le courant
			if (Physics::dirac_distribution(t_- transmission_delay_ - events_in_.top().get_t()) == 1)
			{
                J_ += tau_/membrane_resistance_ * events_in_.top().get_J();
				events_in_.pop();
			}
		}
	}
}


Physics::Time Neuron::get_t() const
{
	return t_;
	
}



double Neuron::get_Vm() const
{
	return Vm_;

}

double Neuron::get_I() const
{
    return J_;

}

int Neuron::get_synapses_size() const
{
	return synapses_.size();
}

int Neuron::get_event_in_size() const
{
	return events_in_.size();
}
