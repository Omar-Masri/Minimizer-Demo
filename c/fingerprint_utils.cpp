#include <sstream>
#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include <tuple>
#include <algorithm>
#include <mutex>

#include "fingerprint_utils.h"
#include "factorization_comb.h"

using namespace std;

mutex mtx;

tuple<string, string> read_long_fasta(string l_1, string l_2){
        string read_original = ""; //variabile per memorizzare la sequenza originale
        string read_rc = ""; //Variabile per memorizzare la sequenza R&C
        string id_gene = ""; //Variabile per tenere traccia del passo di iterazione

        l_1.erase(l_1.begin()); // Rimuovi il carattere '>' dall'ID_GENE
        id_gene = l_1;

        read_original += id_gene + "_0 "; //originale
        read_rc += id_gene + "_1 "; //reverse

        transform(l_2.begin(), l_2.end(), l_2.begin(), ::toupper); // Trasforma la sequenza in maiuscolo

        read_original += l_2;  //concatena ID e sequenza originale
        read_rc += reverse_complement(l_2) + "\n"; // concatena ID e sequenza reverse&complement
        read_original += "\n";

        return make_tuple(read_original, read_rc);
}

// vector<string> read_long_fasta_2_steps(vector<string> fasta_lines) {
//     vector<string> lines; //Lista per contenere le righe processate
//     int i = 0;

//     while (true) {
//         string read_original = ""; //variabile per memorizzare la sequenza originale
//         string read_rc = ""; //Variabile per memorizzare la sequenza R&C
//         string id_gene = ""; //Variabile per tenere traccia del passo di iterazione
//         // ID_GENE
//         string l_1 = fasta_lines[i];

//         l_1.erase(l_1.begin()); // Rimuovi il carattere '>' dall'ID_GENE
//         id_gene = l_1;

//         read_original += id_gene + "_0 "; //originale
//         read_rc += id_gene + "_1 "; //reverse

//         // Read
//         string l_2 = fasta_lines[i + 1]; // Leggi la riga della sequenza

//         read_original += l_2;  //concatena ID e sequenza originale
//         read_rc += reverse_complement(l_2); // concatena ID e sequenza reverse&complement

//         lines.push_back(read_original);  // Aggiungi la sequenza originale alla lista delle righe processate
//         lines.push_back(read_rc);  // Aggiungi la sequenza R&C alla lista delle righe processate

//         i += 2;
//         if (i >= fasta_lines.size()) {
//             break;
//         }
//     }

//     return lines;

// }


//Suddivide le lunghe letture in sottolunghezze
vector<string> factors_string(const string& str, int size = 300) {    
    vector<string> list_of_factors; // Lista per contenere le sottolunghezze

    // Se la lunghezza della stringa è minore della dimensione desiderata, aggiungila direttamente alla lista
    if (str.length() < size) {
        list_of_factors.push_back(str);
    } else {
        // Altrimenti, suddividi la stringa in sottosequenze della dimensione specificata
        for (size_t i = 0; i < str.length(); i += size) {
            string fact;
            // Se l'indice finale supera la lunghezza della stringa, prendi solo i caratteri rimanenti
            if (i + size > str.length()) {
                fact = str.substr(i);
            } else { // Altrimenti, prendi una sottostringa della dimensione specificata
                fact = str.substr(i, size);
            }
            list_of_factors.push_back(fact); // Aggiungi la sottosequenza alla lista delle sottosequenze
        }
    }

     return list_of_factors;

}


string compute_long_fingerprint(string s, int T = 30) {
    string id_gene = ""; // Variabile per l'ID del gene

    istringstream riga(s);
    string read;
    /*La prima operazione iss >> id_gene estrae una stringa dall'iss e la memorizza nella variabile id_gene.
    La seconda operazione iss >> read estrae un'altra stringa dall'iss e la memorizza nella variabile read. */
    riga >> id_gene >> read; // Estrae l'ID del gene e la sequenza di nucleotidi dalla riga

    string lbl_id_gene = id_gene + " "; // Etichetta con l'ID del gene
    string new_line = lbl_id_gene + " "; // Nuova riga per le fingerprint
    string new_fact_line = lbl_id_gene + " "; // Nuova riga per le fingerprint con fattorizzazione

    vector<string> list_of_factors = factors_string(read, 300); // Suddivide la lettura in sottolunghezze di dimensione 300
    for (const auto& sft : list_of_factors) { // Itera su ogni sottolunghezza
        vector<string> list_fact = d_duval_(sft, T); // Applica la tecnica di fattorizzazione alla sottolunghezza

        // Aggiunge le lunghezze delle fingerprint alla riga delle fingerprint
        for (const auto& fact : list_fact) {
            new_line += to_string(fact.length()) + " ";
        }
        new_line += "| ";

    }

    new_line += "\n"; // Aggiunge una nuova riga per le fingerprint
    
    return new_line;
}


// Funzione per leggere il file FASTA, estrarre le letture e restituire la lista delle letture
void extract_long_reads(Args args, string name_file, int remainder) {
    ofstream fingerprint_file;
    if(args.debug != "YES"){
        fingerprint_file.open(args.path + "fingerprint_" + args.type_factorization + ".txt", std::ios::app); //viene aperto automaticamente quando viene istanziato l'oggetto
        std::cout.rdbuf(fingerprint_file.rdbuf());
    }
    ifstream file(name_file);

    string riga;
    string oldriga;
    int i = 0;
    while (getline(file, riga)) { // Legge il file riga per riga
        if (!riga.empty()) { // Se la riga non è vuota, la aggiunge alla lista delle righe del file
            if((i % (args.n * 2)) / 2 == remainder){
                if(i % 2 == 1){
                    string original, rc;
                    tie(original, rc) = read_long_fasta(oldriga, riga);
                    string f_original = compute_long_fingerprint(original, 30);
                    string f_rc = compute_long_fingerprint(rc, 30);

                    mtx.lock();

                    cout << f_original << std::flush;
                    cout << f_rc << std::flush;

                    mtx.unlock();
                }
                oldriga = riga;
            }

            i++;
        }
    }

    // if (!i) {
    //     std::cout << "No reads extracted!" << endl;
    //     exit(-1);
    // }

    file.close();
    fingerprint_file.close();
}

