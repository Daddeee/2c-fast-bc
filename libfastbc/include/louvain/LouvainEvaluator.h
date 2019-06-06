#ifndef FASTBC_LOUVAIN_LOUVAINEVALUATOR_H
#define FASTBC_LOUVAIN_LOUVAINEVALUATOR_H

#include <louvain/ILouvainEvaluator.h>
#include <louvain/LouvainGraph.h>
#include <louvain/Partition.h>
#include <louvain/Community.h>
#include <ctime>
#include <omp.h>

namespace fastbc {
	namespace louvain {
			
		template<typename V, typename W>
		class LouvainEvaluator : public ILouvainEvaluator<V, W>
		{
	 
		private:
			typedef std::vector<std::shared_ptr<ICommunity<V,W>>> Result;
			typedef std::shared_ptr<IDegreeGraph<V,W>> Graph;
			bool verbose = true;	
			double precision = 0.01;	
			int parallelism = 4;

			void
			display_time(const char *str) {
				time_t rawtime;
				time ( &rawtime );
				std::cout << str << ": " << ctime (&rawtime);
			}

			void
			renumber_communities(std::vector<V>& comms, const std::vector<V>& n2c) {
				    std::vector<int> renumber(n2c.size(), -1);
				    for (int node=0 ; node<n2c.size() ; node++) {
				        renumber[n2c[node]]++;
				    }

				    int final=0;
				    for (int i=0 ; i<n2c.size() ; i++)
				        if (renumber[i]!=-1)
				            renumber[i]=final++;

				    for(int i=0; i<comms.size(); i++) {
				    	int old_community = comms[i];
				    	int new_community = n2c[old_community];
				    	int new_renumbered_community = renumber[new_community];

				    	comms[i] = new_renumbered_community;
				    }
			}

			Result
			build_result(const std::vector<int>& n2c, const Graph g) {
				Result r;

				int max = 0;
				for(int i=0; i<n2c.size(); i++)
					if(n2c[i] > max)
						max = n2c[i];

				r.resize(max + 1);
				for(int i=0; i<r.size(); i++)
					r[i] = std::make_shared<Community<V, W>>(g);

				for(int i=0; i<n2c.size(); i++)
					r[n2c[i]]->add(i);

				return r;
			}


		public:
			LouvainEvaluator() {}		
				
			Result evaluateGraph(Graph graph) override
			{
			    time_t time_begin, time_end;
			    time(&time_begin);
			    if (verbose)
			        display_time("Begin");

			    LouvainGraph<V, W> g(graph);
			    std::vector<Partition<V, W> > p(parallelism, Partition<V, W>(g, precision));
			    std::vector<V> n2c(g.nb_nodes);
			    for(int i=0; i<g.nb_nodes; i++) n2c[i] = i;
			    std::vector<bool> improvements(parallelism, true);
			    std::vector<double> modularities(parallelism);
			    int best_i = 0;

			    bool improvement;
			    double mod=p[best_i].modularity(), new_mod;
			    int level=0;

			    do {
			        if (verbose) {
			            std::cout << "level " << level << ":\n";
			            display_time("    start computation");
			            std::cout << "    network size: " 
				     << g.nb_nodes << " nodes, " 
				     << g.nb_links << " links, "
				     << g.total_weight << " weight." << std::endl;
			        }

			        #pragma omp parallel for num_threads(parallelism)
			        for(int i=0; i<parallelism; i++) {
			        	improvements[i] = p[i].one_level();
			        	modularities[i] = p[i].modularity();
			        }

			        int best_i = 0;
			        for(int i=1; i<parallelism; i++)
			        	if(modularities[i] > modularities[best_i])
			        		best_i = i;

			        improvement = improvements[best_i];
			        new_mod = p[best_i].modularity();
			        g = p[best_i].partition2graph();
			        renumber_communities(n2c, p[best_i].n2c);
			        for(int i=0; i<parallelism; i++)
			        	p[i] = Partition<V, W> (g, precision);

			        if (verbose)
			            std::cout << "  modularity increased from " << mod << " to " << new_mod << std::endl;

			        mod=new_mod;
			        if (verbose)
			            display_time("  end computation");
			       level++;
			    } while(improvement);

			    time(&time_end);
			    if (verbose) {
			        display_time("End");
			        std::cout << "Total duration: " << (time_end-time_begin) << " sec." << std::endl; 
			    }
			    std::cout << new_mod << std::endl;
			    std::cout << std::endl;

				return build_result(n2c, graph);
			}
		};
	}
}


#endif
