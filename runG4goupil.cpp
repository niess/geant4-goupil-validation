#include "DetectorConstruction.hh"
#include "PhysicsList.hh"
#include "PrimaryGenerator.hh"
#include "SteppingAction.hh"
/* Geant4 interface */
#include "G4EmCalculator.hh"
#include "G4Gamma.hh"
#include "G4RunManagerFactory.hh"


struct header {
    char model[32];
    double energy;
    long events;
};

struct parameters {
    char help[20];
    struct header header;
    char outputFile[255];
    parameters() : help(""), outputFile("geant4-goupil-validation.bin") {
        std::strcpy(header.model, "standard");
        header.energy = 1;
        header.events = 1000000;
    }
};



struct parameters getParams(int argc, char** argv);
void show_usage(const char* name);

int main(int argc, char **argv) {
    struct parameters params = getParams(argc, argv);
    if(strcmp(params.help, "-h") == 0 || strcmp(params.help, "--help") == 0 ||
            strcmp(params.header.model, "") == 0 ||
            params.header.energy <= 0 || params.header.events <= 0 ||
            strcmp(params.outputFile, "") == 0) {
        show_usage(argv[0]);
        return -1;
    }
    
    std::cout << "=== simulation parameters ===" << std::endl
            << "model      : " << params.header.model << std::endl
            << "energy     : " << params.header.energy << " MeV" << std::endl
            << "events     : " << params.header.events << std::endl
            << "output file: " << params.outputFile << std::endl;
    
    FILE * stream = fopen(params.outputFile, "wb");
    
    if(stream) {
        fwrite(&params.header, sizeof(params.header), 1, stream);
        fclose(stream);
    }
    else {
        std::cerr << "Could not open file " << params.outputFile << std::endl;
        exit(EXIT_FAILURE);
    }
    
    auto buffer = std::cout.rdbuf();
    std::cout.rdbuf(nullptr); /* Disable cout temporarly. */
    /* Construct the default run manager */
    auto * runManager = G4RunManagerFactory::CreateRunManager(G4RunManagerType::Default);
    std::cout.rdbuf(buffer);
    
    /* Set mandatory initialization classes */
    runManager->SetUserInitialization(DetectorConstruction::Singleton());
    auto && physics = PhysicsList::Singleton(params.header.model);
    runManager->SetUserInitialization(physics);
    physics->DisableVerbosity();
    
    auto && generator = PrimaryGenerator::Singleton();
    runManager->SetUserAction(generator);
    runManager->SetUserAction(new SteppingAction(params.outputFile));
    
    runManager->Initialize();
    
    generator->event->energy = params.header.energy;
    
    runManager->BeamOn(params.header.events);
    
    // Dump cross-sections.
    G4EmCalculator emCal;
    auto particle = G4Gamma::GammaDefinition();
    G4ProcessVector& processes = *particle->GetProcessManager()->GetProcessList();
    G4double eMin = 1E-03 * CLHEP::MeV;
    G4double eMax = 1E+01 * CLHEP::MeV;
    int ne = 401;
    double re = std::log(eMax / eMin) / (ne - 1);
    auto material = DetectorConstruction::Singleton()->material;
    
    auto filename = std::string("share/data/cross-sections-") + params.header.model + std::string(".txt");
    stream = std::fopen(filename.c_str(), "w+");
    fprintf(stream, "# energy");
    for (size_t j = 0; j < processes.size(); j++) {
        G4VProcess * proc = processes[G4int(j)];
        if (proc->GetProcessName() == "Transportation") continue;
        fprintf(stream, " %s", proc->GetProcessName().c_str());
    }
    fprintf(stream, "\n");
    
    for (int i = 0; i < ne; i++) {
        G4double energy = eMin * std::exp(i * re);
        fprintf(stream, "%.5E", energy);
        for (size_t j = 0; j < processes.size(); j++) {
            G4VProcess * proc = processes[G4int(j)];
            if (proc->GetProcessName() == "Transportation") continue;
            G4double sigma = emCal.GetCrossSectionPerVolume(
                energy, particle, proc->GetProcessName(), material) / material->GetTotNbOfAtomsPerVolume();
            fprintf(stream, " %.5E", sigma / CLHEP::barn);
        }
        fprintf(stream, "\n");
    }
    std::fclose(stream);
    
    delete runManager;
    return 0;
}



struct parameters getParams(int argc, char** argv) {
    struct parameters params;
    for(int i = 1; i < argc; i += 2) {
        if(strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            std::strcpy(params.help, "-h");
        }
        if(strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--model") == 0) {
            std::strcpy(params.header.model, argv[i + 1]);
        }
        if(strcmp(argv[i], "-e") == 0 || strcmp(argv[i], "--energy") == 0) {
            params.header.energy = std::stod(argv[i + 1]);
        }
        if(strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "--events") == 0) {
            params.header.events = std::stol(argv[i + 1]);
        }
        if(strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) {
            std::strcpy(params.outputFile, argv[i + 1]);
        }
    }
    return params;
}

void show_usage(const char* name) {
    std::cerr << "Usage: " << name << " <option(s)> SOURCES" << std::endl
            << "Options:" << std::endl
            << "\t-h,--help\tShow this help message" << std::endl
            << "\t-m,--model\tSpecify the physics model" << std::endl
            << "\t-e,--energy\tSpecify the kinetic energy in [MeV]" << std::endl
            << "\t-n,--events\tSpecify the number of events to generate" << std::endl
            << "\t-o,--output\tSpecify the output file"
            << std::endl;
}
