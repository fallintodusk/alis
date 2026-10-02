//! Topological sorting algorithm (Kahn's algorithm)

use anyhow::{anyhow, Result};
use std::collections::{HashMap, HashSet, VecDeque};

/// Topological sorter using Kahn's algorithm
pub struct TopologicalSorter;

impl TopologicalSorter {
    pub fn new() -> Self {
        Self
    }

    /// Perform topological sort on a dependency graph
    ///
    /// Returns plugins in an order where dependencies come before dependents.
    /// Detects and reports circular dependencies.
    pub fn sort(&self, graph: &HashMap<String, Vec<String>>) -> Result<Vec<String>> {
        if graph.is_empty() {
            return Ok(Vec::new());
        }

        // Build reverse adjacency list (dependents map) - O(E) one-time cost
        let mut reverse_adj: HashMap<String, Vec<String>> = HashMap::new();
        let mut all_nodes = HashSet::new();

        for (node, deps) in graph {
            all_nodes.insert(node.clone());
            reverse_adj.entry(node.clone()).or_insert_with(Vec::new);

            for dep in deps {
                all_nodes.insert(dep.clone());
                reverse_adj
                    .entry(dep.clone())
                    .or_insert_with(Vec::new)
                    .push(node.clone());
            }
        }

        // Calculate in-degrees (number of incoming edges)
        let mut in_degree: HashMap<String, usize> = all_nodes
            .iter()
            .map(|node| (node.clone(), 0))
            .collect();

        for (node, deps) in graph {
            *in_degree.entry(node.clone()).or_insert(0) += deps.len();
        }

        // Queue nodes with no incoming edges (sorted for deterministic output)
        let mut zero_degree: Vec<String> = in_degree
            .iter()
            .filter(|(_node, &degree)| degree == 0)
            .map(|(node, _)| node.clone())
            .collect();
        zero_degree.sort();
        let mut queue: VecDeque<String> = zero_degree.into();

        let mut sorted = Vec::new();

        while let Some(node) = queue.pop_front() {
            sorted.push(node.clone());

            // Use precomputed reverse adjacency list - O(1) lookup instead of O(V) scan
            if let Some(dependents) = reverse_adj.get(&node) {
                // Sort dependents for deterministic processing order
                let mut sorted_dependents = dependents.clone();
                sorted_dependents.sort();

                for dependent in sorted_dependents {
                    if let Some(degree) = in_degree.get_mut(&dependent) {
                        *degree -= 1;
                        if *degree == 0 {
                            queue.push_back(dependent);
                        }
                    }
                }
            }
        }

        // Check if all nodes were processed (no cycles)
        if sorted.len() != all_nodes.len() {
            // Find nodes involved in cycle
            let unprocessed: Vec<String> = all_nodes
                .iter()
                .filter(|node| !sorted.contains(node))
                .cloned()
                .collect();

            return Err(anyhow!(
                "Circular dependency detected involving: {}",
                unprocessed.join(", ")
            ));
        }

        Ok(sorted)
    }

    /// Detect if the graph contains cycles
    pub fn has_cycle(&self, graph: &HashMap<String, Vec<String>>) -> bool {
        self.sort(graph).is_err()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_topological_sort_simple() {
        let mut graph = HashMap::new();
        graph.insert("A".to_string(), vec!["B".to_string()]);
        graph.insert("B".to_string(), vec!["C".to_string()]);
        graph.insert("C".to_string(), vec![]);

        let sorter = TopologicalSorter::new();
        let result = sorter.sort(&graph);

        assert!(result.is_ok());
        let sorted = result.unwrap();

        // C should come before B, B should come before A
        let c_pos = sorted.iter().position(|x| x == "C").unwrap();
        let b_pos = sorted.iter().position(|x| x == "B").unwrap();
        let a_pos = sorted.iter().position(|x| x == "A").unwrap();

        assert!(c_pos < b_pos);
        assert!(b_pos < a_pos);
    }

    #[test]
    fn test_topological_sort_cycle() {
        let mut graph = HashMap::new();
        graph.insert("A".to_string(), vec!["B".to_string()]);
        graph.insert("B".to_string(), vec!["C".to_string()]);
        graph.insert("C".to_string(), vec!["A".to_string()]); // Cycle!

        let sorter = TopologicalSorter::new();
        let result = sorter.sort(&graph);

        assert!(result.is_err());
        assert!(result.unwrap_err().to_string().contains("Circular dependency"));
    }

    #[test]
    fn test_topological_sort_empty() {
        let graph = HashMap::new();
        let sorter = TopologicalSorter::new();
        let result = sorter.sort(&graph);

        assert!(result.is_ok());
        assert_eq!(result.unwrap().len(), 0);
    }

    #[test]
    fn test_has_cycle() {
        let mut graph = HashMap::new();
        graph.insert("A".to_string(), vec!["B".to_string()]);
        graph.insert("B".to_string(), vec!["A".to_string()]); // Cycle

        let sorter = TopologicalSorter::new();
        assert!(sorter.has_cycle(&graph));
    }

    #[test]
    fn test_no_cycle() {
        let mut graph = HashMap::new();
        graph.insert("A".to_string(), vec!["B".to_string()]);
        graph.insert("B".to_string(), vec![]);

        let sorter = TopologicalSorter::new();
        assert!(!sorter.has_cycle(&graph));
    }
}
