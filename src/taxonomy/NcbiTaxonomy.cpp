// Ported from blast2lca
// https://github.com/emepyc/Blast2lca
// Originally licensed under GPLv2 or later

#include "NcbiTaxonomy.h"
#include "FileUtil.h"
#include "MathUtil.h"
#include "Debug.h"
#include "Util.h"

#include <fstream>
#include <algorithm>
#include <cassert>

int **makeMatrix(size_t maxNodes) {
    Debug(Debug::INFO) << "Making matrix ...";
    size_t dimension = maxNodes * 2;
    int **M = new int*[dimension];
    int k = (int)(MathUtil::flog2(dimension)) + 1;
    for (size_t i = 0; i < dimension; ++i) {
        M[i] = new int[k]();
    }
    Debug(Debug::INFO) << " Done\n";
    return M;
}

void deleteMatrix(int** M, size_t maxNodes) {
    for (size_t i = 0; i < (maxNodes * 2); ++i) {
        delete[] M[i];
    }
    delete[] M;
}


NcbiTaxonomy::NcbiTaxonomy(const std::string &namesFile,  const std::string &nodesFile,
                           const std::string &mergedFile) {
    loadNodes(nodesFile);
    loadMerged(mergedFile);
    loadNames(namesFile);

    maxNodes = taxonNodes.size();

    E.reserve(maxNodes * 2);
    L.reserve(maxNodes * 2);

    H = new int[maxNodes];
    std::fill(H, H + maxNodes, 0);

    std::vector< std::vector<TaxID> > children(taxonNodes.size());
    for (std::vector<TaxonNode>::iterator it = taxonNodes.begin(); it != taxonNodes.end(); ++it) {
        if (it->parentTaxId != it->taxId) {
            children[nodeId(it->parentTaxId)].push_back(it->taxId);
        }
    }

    elh(children, 1, 0);
    E.resize(maxNodes * 2, 0);
    L.resize(maxNodes * 2, 0);

    M = makeMatrix(maxNodes);
    InitRangeMinimumQuery();
}

NcbiTaxonomy::~NcbiTaxonomy() {
    delete[] H;
    deleteMatrix(M, maxNodes);
}

std::vector<std::string> splitByDelimiter(const std::string &s, const std::string &delimiter, int maxCol) {
    std::vector<std::string> result;
    size_t prev = 0, pos = 0;
    int i = 0;
    do {
        pos = s.find(delimiter, prev);
        if (pos == std::string::npos) pos = s.length();
        result.emplace_back(s.substr(prev, pos - prev));
        prev = pos + delimiter.length();
        i++;
    } while (pos < s.length() && prev < s.length() && i < maxCol);

    return result;
}

size_t NcbiTaxonomy::loadNodes(const std::string &nodesFile) {
    Debug(Debug::INFO) << "Loading nodes file ...";
    std::ifstream ss(nodesFile);
    if (ss.fail()) {
        Debug(Debug::ERROR) << "File " << nodesFile << " not found!\n";
        EXIT(EXIT_FAILURE);
    }

    std::map<TaxID, int> Dm; // temporary map TaxID -> internal ID;
    int maxTaxID = 0;
    int currentId = 0;
    std::string line;
    while (std::getline(ss, line)) {
        std::vector<std::string> result = splitByDelimiter(line, "\t|\t", 3);
        TaxID taxId = (TaxID) strtol(result[0].c_str(), NULL, 10);
        TaxID parentTaxId = (TaxID) strtol(result[1].c_str(), NULL, 10);
        if (taxId > maxTaxID) {
            maxTaxID = taxId;
        }
        taxonNodes.emplace_back(currentId, taxId, parentTaxId, result[2]);
        Dm.emplace(taxId, currentId);
        ++currentId;
    }

    D.clear();
    D.resize(maxTaxID + 1, -1);
    for (std::map<TaxID, int>::iterator it = Dm.begin(); it != Dm.end(); ++it) {
        assert(it->first <= maxTaxID);
        D[it->first] = it->second;
    }

    // Loop over taxonNodes and check all parents exist
    for (std::vector<TaxonNode>::iterator it = taxonNodes.begin(); it != taxonNodes.end(); ++it) {
        if (!nodeExists(it->parentTaxId)) {
            Debug(Debug::ERROR) << "Inconsistent nodes.dmp taxonomy file! Cannot find parent taxon with ID " << it->parentTaxId << "!\n";
            EXIT(EXIT_FAILURE);
        }
    }

    Debug(Debug::INFO) << " Done, got " << taxonNodes.size() << " nodes\n";
    return taxonNodes.size();
}

std::pair<int, std::string> parseName(const std::string &line) {
    std::vector<std::string> result = splitByDelimiter(line, "\t|\t", 2);
    if (result.size() != 2) {
        Debug(Debug::ERROR) << "Invalid name entry!\n";
        EXIT(EXIT_FAILURE);
    }
    return std::make_pair((int)strtol(result[0].c_str(), NULL, 10), result[1]);
}

void NcbiTaxonomy::loadNames(const std::string &namesFile) {
    Debug(Debug::INFO) << "Loading names file ...";
    std::ifstream ss(namesFile);
    if (ss.fail()) {
        Debug(Debug::ERROR) << "File " << namesFile << " not found!\n";
        EXIT(EXIT_FAILURE);
    }

    std::string line;
    while (std::getline(ss, line)) {
        if (line.find("scientific name") == std::string::npos) {
            continue;
        }

        std::pair<int, std::string> entry = parseName(line);
        if (!nodeExists(entry.first)) {
            Debug(Debug::ERROR) << "loadNames: Taxon " << entry.first << " not present in nodes file!\n";
            EXIT(EXIT_FAILURE);
        }
        taxonNodes[nodeId(entry.first)].name = entry.second;
    }
    Debug(Debug::INFO) << " Done\n";
}

// Euler traversal of tree
void NcbiTaxonomy::elh(std::vector< std::vector<TaxID> > const & children, TaxID taxId, int level) {
    assert (taxId > 0);
    int id = nodeId(taxId);

    if (H[id] == 0) {
        H[id] = E.size();
    }

    E.emplace_back(id);
    L.emplace_back(level);

    for (std::vector<TaxID>::const_iterator child_it = children[id].begin(); child_it != children[id].end(); ++child_it) {
        elh(children, *child_it, level + 1);
    }
    E.emplace_back(nodeId(taxonNodes[id].parentTaxId));
    L.emplace_back(level - 1);
}

void NcbiTaxonomy::InitRangeMinimumQuery() {
    Debug(Debug::INFO) << "Init RMQ ...";

    for (unsigned int i = 0; i < (maxNodes * 2); ++i) {
        M[i][0] = i;
    }

    for (unsigned int j = 1; (1ul << j) <= (maxNodes * 2); ++j) {
        for (unsigned int i = 0; (i + (1ul << j) - 1) < (maxNodes * 2); ++i) {
            int A = M[i][j - 1];
            int B = M[i + (1ul << (j - 1))][j - 1];
            if (L[A] < L[B]) {
                M[i][j] = A;
            } else {
                M[i][j] = B;
            }
        }
    }
    Debug(Debug::INFO) << "Done\n";
}

int NcbiTaxonomy::RangeMinimumQuery(int i, int j) const {
    assert(j >= i);
    int k = (int)MathUtil::flog2(j - i + 1);
    int A = M[i][k];
    int B = M[j - MathUtil::ipow<int>(2, k) + 1][k];
    if (L[A] <= L[B]) {
        return A;
    }
    return B;
}

int NcbiTaxonomy::lcaHelper(int i, int j) const {
    if (i == 0 || j == 0) {
        return 0;
    }
    assert(i > 0);
    assert(j > 0);
    if (i == j) {
        return i;
    }
    int v1 = H[i];
    int v2 = H[j];
    if (v1 > v2) {
        int tmp = v1;
        v1 = v2;
        v2 = tmp;
    }
    int rmq = RangeMinimumQuery(v1, v2);
    assert(E[rmq] >= 0);
    return E[rmq];
}

bool NcbiTaxonomy::IsAncestor(TaxID ancestor, TaxID child) {
    if (ancestor == child) {
        return true;
    }

    if (ancestor == 0 || child == 0) {
        return false;
    }

    if (!nodeExists(child)) {
        Debug(Debug::WARNING) << "No node for taxID " << child << ".\n";
        return false;
    } 

    if (!nodeExists(ancestor)) {
        Debug(Debug::WARNING) << "No node for taxID " << ancestor << ".\n";
        return false;
    }

    return lcaHelper(nodeId(child), nodeId(ancestor)) == nodeId(ancestor);
}


TaxID NcbiTaxonomy::LCA(TaxID taxonA, TaxID taxonB) const {
    if (!nodeExists(taxonA)) {
        return taxonB;
    } else if (!nodeExists(taxonB)) {
        return taxonA;
    }
    return taxonNodes[lcaHelper(nodeId(taxonA), nodeId(taxonB))].taxId;
}


TaxonNode const * NcbiTaxonomy::LCA(const std::vector<TaxID>& taxa) const {
    std::vector<int>::const_iterator it = taxa.begin();
    while (it != taxa.end() && !nodeExists(*it)) {
        Debug(Debug::WARNING) << "No node for taxID " << *it << ", ignoring it.\n";
        ++it;
    }
    if (it == taxa.end()) { return NULL; }
    int red = nodeId(*it++);
    for (; it != taxa.end(); ++it) {
        if (nodeExists(*it)) {
            red = lcaHelper(red, nodeId(*it));
        } else {
            Debug(Debug::WARNING) << "No node for taxID " << *it << ", ignoring it.\n";
        }
    }

    assert(red >= 0 && static_cast<unsigned int>(red) < taxonNodes.size());

    return &(taxonNodes[red]);
}


// AtRanks returns a slice of slices having the taxons at the specified taxonomic levels
std::vector<std::string> NcbiTaxonomy::AtRanks(TaxonNode const *node, const std::vector<std::string> &levels) const {
    std::vector<std::string> result;
    std::map<std::string, std::string> allRanks = AllRanks(node);
    // map does not include "no rank" nor "no_rank"
    int baseRankIndex = findRankIndex(node->rank);
    std::string baseRank = "uc_" + node->name;
    for (std::vector<std::string>::const_iterator it = levels.begin(); it != levels.end(); ++it) {
        std::map<std::string, std::string>::iterator jt = allRanks.find(*it);
        if (jt != allRanks.end()) {
            result.emplace_back(jt->second);
            continue;
        }

        // If not ... 2 possible causes: i) too low level ("uc_")
        if (NcbiRanks.at(*it) < baseRankIndex) {
            result.emplace_back(baseRank);
            continue;
        }

        // ii) No taxon for the LCA at the required level -- give the first known upstream
        result.emplace_back("unknown");
    }
    return result;
}

std::vector<std::string> NcbiTaxonomy::parseRanks(const std::string& ranks) {
    std::vector<std::string> temp = Util::split(ranks, ",");
    for (size_t i = 0; i < temp.size(); ++i) {
        if (findRankIndex(temp[i]) == -1) {
            Debug(Debug::ERROR) << "Invalid taxonomic rank " << temp[i] << "given\n";
            EXIT(EXIT_FAILURE);
        }
    }
    return temp;
}

int NcbiTaxonomy::findRankIndex(const std::string& rank) {
    std::map<std::string, int>::const_iterator it;
    if ((it = NcbiRanks.find(rank)) != NcbiRanks.end()) {
        return it->second;
    }
    return -1;
}

char NcbiTaxonomy::findShortRank(const std::string& rank) {
    std::map<std::string, char>::const_iterator it;
    if ((it = NcbiShortRanks.find(rank)) != NcbiShortRanks.end()) {
        return it->second;
    }
    return '-';
}

std::string NcbiTaxonomy::taxLineage(TaxonNode const *node, bool infoAsName) {
    std::vector<TaxonNode const *> taxLineageVec;
    std::string taxLineage;
    taxLineage.reserve(4096);
    do {
        taxLineageVec.push_back(node);
        node = taxonNode(node->parentTaxId);
    } while (node->parentTaxId != node->taxId);

    for (int i = taxLineageVec.size() - 1; i >= 0; --i) {
        if (infoAsName) {
            taxLineage += findShortRank(taxLineageVec[i]->rank);
            taxLineage += '_';
            taxLineage += taxLineageVec[i]->name;
        } else {
            taxLineage += SSTR(taxLineageVec[i]->taxId);
        }
        
        if (i > 0) {
            taxLineage += ";";
        }
    }
    return taxLineage;
}

int NcbiTaxonomy::nodeId(TaxID taxonId) const {
    if (taxonId < 0 || !nodeExists(taxonId)) {
        Debug(Debug::ERROR) << "Invalid node " << taxonId << "!\n";
        EXIT(EXIT_FAILURE);
    }
    return D[taxonId];
}

bool NcbiTaxonomy::nodeExists(TaxID taxonId) const {
    return D[taxonId] != -1;
}

TaxonNode const * NcbiTaxonomy::taxonNode(TaxID taxonId, bool fail) const {
    if (taxonId == 0 || (!fail && !nodeExists(taxonId))) {
        return NULL;
    }
    return &(taxonNodes[nodeId(taxonId)]);
}

std::map<std::string, std::string> NcbiTaxonomy::AllRanks(TaxonNode const *node) const {
    std::map<std::string, std::string> result;
    while (true) {
        if (node->taxId == 1) {
            result.emplace(node->rank, node->name);
            return result;
        }

        if ((node->rank != "no_rank") && (node->rank != "no rank")) {
            result.emplace(node->rank, node->name);
        }

        node = taxonNode(node->parentTaxId);
    }
}

size_t NcbiTaxonomy::loadMerged(const std::string &mergedFile) {
    Debug(Debug::INFO) << "Loading merged file ...";
    std::ifstream ss(mergedFile);
    if (ss.fail()) {
        Debug(Debug::ERROR) << "File " << mergedFile << " not found!\n";
        EXIT(EXIT_FAILURE);
    }

    std::string line;
    size_t count = 0;
    while (std::getline(ss, line)) {
        std::vector<std::string> result = splitByDelimiter(line, "\t|\t", 2);
        if (result.size() != 2) {
            Debug(Debug::ERROR) << "Invalid name entry!\n";
            EXIT(EXIT_FAILURE);
        }

        unsigned int oldId = (unsigned int)strtoul(result[0].c_str(), NULL, 10);
        unsigned int mergedId = (unsigned int)strtoul(result[1].c_str(), NULL, 10);
        if (!nodeExists(oldId) && nodeExists(mergedId)) {
            D[oldId] = D[mergedId];
            ++count;
        }
    }
    Debug(Debug::INFO) << " Done, added " << count << " merged nodes.\n";
    return count;
}

std::unordered_map<TaxID, TaxonCounts> NcbiTaxonomy::getCladeCounts(std::unordered_map<TaxID, unsigned int>& taxonCounts) const {
    Debug(Debug::INFO) << "Calculating clade counts ... ";
    std::unordered_map<TaxID, TaxonCounts> cladeCounts;

    for (std::unordered_map<TaxID, unsigned int>::const_iterator it = taxonCounts.begin(); it != taxonCounts.end(); ++it) {
        cladeCounts[it->first].taxCount = it->second;
        cladeCounts[it->first].cladeCount += it->second;
        if (nodeExists(it->first)) {
            TaxonNode const* taxon = taxonNode(it->first);
            while (taxon->parentTaxId != taxon->taxId && nodeExists(taxon->parentTaxId)) {
                taxon = taxonNode(taxon->parentTaxId);
                cladeCounts[taxon->taxId].cladeCount += it->second;
            }
        }
    }

    for (const TaxonNode& tn : taxonNodes) {
        if (tn.parentTaxId != tn.taxId && cladeCounts.count(tn.taxId)) {
            std::unordered_map<TaxID, TaxonCounts>::iterator itp = cladeCounts.find(tn.parentTaxId);
            itp->second.children.push_back(tn.taxId);
        }
    }

    Debug(Debug::INFO) << " Done\n";
    return cladeCounts;
}

NcbiTaxonomy * NcbiTaxonomy::openTaxonomy(std::string &database){
    Debug(Debug::INFO) << "Loading NCBI taxonomy\n";
    std::string nodesFile = database + "_nodes.dmp";
    std::string namesFile = database + "_names.dmp";
    std::string mergedFile = database + "_merged.dmp";
    if (FileUtil::fileExists(nodesFile.c_str())
        && FileUtil::fileExists(namesFile.c_str())
        && FileUtil::fileExists(mergedFile.c_str())) {
    } else if (FileUtil::fileExists("nodes.dmp")
               && FileUtil::fileExists("names.dmp")
               && FileUtil::fileExists("merged.dmp")) {
        nodesFile = "nodes.dmp";
        namesFile = "names.dmp";
        mergedFile = "merged.dmp";
    } else {
        Debug(Debug::ERROR) << "names.dmp, nodes.dmp, merged.dmp from NCBI taxdump could not be found!\n";
        EXIT(EXIT_FAILURE);
    }
    return new NcbiTaxonomy(namesFile, nodesFile, mergedFile);
}

const TaxID ROOT_TAXID = 1;
const int ROOT_RANK = INT_MAX;

struct TaxNode {
    TaxNode(const double weight, const bool isCandidate, const TaxID childTaxon)
            : weight(weight), isCandidate(isCandidate), childTaxon(childTaxon) {}

    void update(const double weightToAdd, const TaxID & childTaxonInput) {
        if (childTaxon != childTaxonInput) {
            isCandidate = true;
            childTaxon = childTaxonInput;
        }
        weight += weightToAdd;
    }

    double weight;
    bool isCandidate;
    TaxID childTaxon;
};

WeightedTaxHit::WeightedTaxHit(const TaxID taxon, const float evalue, const int weightVoteMode) : taxon(taxon) {
    switch (weightVoteMode) {
        case Parameters::AGG_TAX_UNIFORM:
            weight = 1.0;
            break;
        case Parameters::AGG_TAX_MINUS_LOG_EVAL:
            weight = evalue;
            if (evalue != FLT_MAX) {
                if (evalue > 0) {
                    weight = -log(evalue);
                } else {
                    weight = MAX_TAX_WEIGHT;
                }
            }
            break;
        case Parameters::AGG_TAX_SCORE:
            weight = evalue;
            break;
        default:
            Debug(Debug::ERROR) << "Invalid weight vote mode\n";
            EXIT(EXIT_FAILURE);
    }
}

WeightedTaxResult NcbiTaxonomy::weightedMajorityLCA(const std::vector<WeightedTaxHit> &setTaxa, const float majorityCutoff) {
    // count num occurences of each ancestor, possibly weighted
    std::map<TaxID, TaxNode> ancTaxIdsCounts;

    // initialize counters and weights
    size_t assignedSeqs = 0;
    size_t unassignedSeqs = 0;
    size_t seqsAgreeWithSelectedTaxon = 0;
    double selectedPercent = 0;
    double totalAssignedSeqsWeights = 0.0;

    for (size_t i = 0; i < setTaxa.size(); ++i) {
        TaxID currTaxId = setTaxa[i].taxon;
        double currWeight = setTaxa[i].weight;
        // ignore unassigned sequences
        if (currTaxId == 0) {
            unassignedSeqs++;
            continue;
        }
        TaxonNode const *node = taxonNode(currTaxId, false);
        if (node == NULL) {
            Debug(Debug::ERROR) << "taxonid: " << currTaxId << " does not match a legal taxonomy node.\n";
            EXIT(EXIT_FAILURE);
        }
        totalAssignedSeqsWeights += currWeight;
        assignedSeqs++;

        // each start of a path due to an orf is a candidate
        std::map<TaxID, TaxNode>::iterator it;
        if ((it = ancTaxIdsCounts.find(currTaxId)) != ancTaxIdsCounts.end()) {
            it->second.update(currWeight, 0);
        } else {
            TaxNode current(currWeight, true, 0);
            ancTaxIdsCounts.emplace(currTaxId, current);
        }

        // iterate all ancestors up to root (including). add currWeight and candidate status to each
        TaxID currParentTaxId = node->parentTaxId;
        while (currParentTaxId != currTaxId) {
            if ((it = ancTaxIdsCounts.find(currParentTaxId)) != ancTaxIdsCounts.end()) {
                it->second.update(currWeight, currTaxId);
            } else {
                TaxNode parent(currWeight, false, currTaxId);
                ancTaxIdsCounts.emplace(currParentTaxId, parent);
            }
            // move up:
            currTaxId = currParentTaxId;
            node = taxonNode(currParentTaxId, false);
            currParentTaxId = node->parentTaxId;
        }
    }

    // select the lowest ancestor that meets the cutoff
    int minRank = INT_MAX;
    TaxID selctedTaxon = 0;

    for (std::map<TaxID, TaxNode>::iterator it = ancTaxIdsCounts.begin(); it != ancTaxIdsCounts.end(); it++) {
        // consider only candidates:
        if (it->second.isCandidate == false) {
            continue;
        }

        double currPercent = float(it->second.weight) / totalAssignedSeqsWeights;
        if (currPercent >= majorityCutoff) {
            // iterate all ancestors to find lineage min rank (the candidate is a descendant of a node with this rank)
            TaxID currTaxId = it->first;
            TaxonNode const *node = taxonNode(currTaxId, false);
            int currMinRank = ROOT_RANK;
            TaxID currParentTaxId = node->parentTaxId;
            while (currParentTaxId != currTaxId) {
                int currRankInd = NcbiTaxonomy::findRankIndex(node->rank);
                if ((currRankInd > 0) && (currRankInd < currMinRank)) {
                    currMinRank = currRankInd;
                    // the rank can only go up on the way to the root, so we can break
                    break;
                }
                // move up:
                currTaxId = currParentTaxId;
                node = taxonNode(currParentTaxId, false);
                currParentTaxId = node->parentTaxId;
            }

            if ((currMinRank < minRank) || ((currMinRank == minRank) && (currPercent > selectedPercent))) {
                selctedTaxon = it->first;
                minRank = currMinRank;
                selectedPercent = currPercent;
            }
        }
    }

    // count the number of seqs who have selectedTaxon in their ancestors (agree with selection):
    if (selctedTaxon == ROOT_TAXID) {
        // all agree with "root"
        seqsAgreeWithSelectedTaxon = assignedSeqs;
        return WeightedTaxResult(selctedTaxon, assignedSeqs, unassignedSeqs, seqsAgreeWithSelectedTaxon, selectedPercent);
    }
    if (selctedTaxon == 0) {
        // nothing informative
        return WeightedTaxResult(selctedTaxon, assignedSeqs, unassignedSeqs, seqsAgreeWithSelectedTaxon, selectedPercent);
    }
    // otherwise, iterate over all seqs
    for (size_t i = 0; i < setTaxa.size(); ++i) {
        TaxID currTaxId = setTaxa[i].taxon;
        // ignore unassigned sequences
        if (currTaxId == 0) {
            continue;
        }
        TaxonNode const *node = taxonNode(currTaxId, false);

        // iterate all ancestors up to the root
        TaxID currParentTaxId = node->parentTaxId;
        while (currParentTaxId != currTaxId) {
            if (currTaxId == selctedTaxon) {
                seqsAgreeWithSelectedTaxon++;
                break;
            }
            currTaxId = currParentTaxId;
            node = taxonNode(currParentTaxId, false);
            currParentTaxId = node->parentTaxId;
        }
    }

    return WeightedTaxResult(selctedTaxon, assignedSeqs, unassignedSeqs, seqsAgreeWithSelectedTaxon, selectedPercent);
}
