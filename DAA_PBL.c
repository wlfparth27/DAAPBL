#include <stdio.h>
#include <stdlib.h>
#define MAX 100

typedef struct {
    int id;
    float weight;
    float priority;
    float ratio;
} Item;


typedef struct {
    int level;              
    float weight;           
    float priority;         
    float bound;            
    int selected[MAX];      
} Node;


Node heap[10000];
int heapSize = 0;


void swap(Node *a, Node *b)
{
    Node temp = *a;
    *a = *b;
    *b = temp;
}


void insert(Node node)
{
    int i = heapSize;
    heap[heapSize++] = node;
    while (i > 0)
    {
        int parent = (i - 1) / 2;
        if (heap[parent].bound >= heap[i].bound)
            break;
        swap(&heap[parent], &heap[i]);
        i = parent;
    }
}


Node removeMax()
{
    Node maxNode = heap[0];
    heap[0] = heap[--heapSize];
    int i = 0;
    while (1)
    {
        int left = 2 * i + 1;
        int right = 2 * i + 2;
        int largest = i;
        if (left < heapSize &&
            heap[left].bound > heap[largest].bound)
        {
            largest = left;
        }
        if (right < heapSize &&
            heap[right].bound > heap[largest].bound)
        {
            largest = right;
        }
        if (largest == i)
            break;
        swap(&heap[i], &heap[largest]);
        i = largest;
    }
    return maxNode;
}


void sortItems(Item items[], int n)
{
    int i, j;
    for (i = 0; i < n - 1; i++)
    {
        for (j = 0; j < n - i - 1; j++)
        {
            if (items[j].ratio < items[j + 1].ratio)
            {
                Item temp = items[j];
                items[j] = items[j + 1];
                items[j + 1] = temp;
            }
        }
    }
}


float calculateBound(Node node, Item items[], int n, float capacity)
{
    if (node.weight > capacity)
        return 0;
    float bound = node.priority;
    float totalWeight = node.weight;
    int i = node.level + 1;
    while (i < n &&
           totalWeight + items[i].weight <= capacity)
    {
        totalWeight += items[i].weight;
        bound += items[i].priority;
        i++;
    }
    if (i < n && totalWeight < capacity)
    {
        float remaining =
            capacity - totalWeight;
        bound += remaining * items[i].ratio;
    }
    return bound;
}


void branchAndBound(Item items[], int n, float capacity)
{
    Node root;
    Node current;
    Node includeNode;
    Node excludeNode;
    float bestPriority = 0;
    float bestWeight = 0;
    int bestSelected[MAX] = {0};
    int nodesExplored = 0;
    int nodesPruned = 0;
    root.level = -1;
    root.weight = 0;
    root.priority = 0;
    for (int i = 0; i < n; i++)
        root.selected[i] = 0;
    root.bound = calculateBound(root, items, n, capacity);
    insert(root);
    printf("\n\nSEARCH TREE TRACE\n");
    printf("-------------------------------------------------------------\n");
    printf("%-7s %-10s %-10s %-10s %-12s\n", "Level", "Weight", "Priority", "Bound", "Status");
    printf("-------------------------------------------------------------\n");
    while (heapSize > 0)
    {
        current = removeMax();
        nodesExplored++;
        printf("%-7d %-10.2f %-10.2f %-10.2f EXPLORED\n", current.level, current.weight, current.priority, current.bound);
        if (current.bound <= bestPriority)
        {
            nodesPruned++;
            continue;
        }
        if (current.level == n - 1)
            continue;
        int next = current.level + 1;


        includeNode = current;
        includeNode.level = next;
        includeNode.weight = current.weight + items[next].weight;
        includeNode.priority = current.priority + items[next].priority;
        includeNode.selected[next] = 1;

        if (includeNode.weight <= capacity)
        {
            if (includeNode.priority > bestPriority)
            {
                bestPriority = includeNode.priority;
                bestWeight = includeNode.weight;
                for (int i = 0; i < n; i++)
                {
                    bestSelected[i] = includeNode.selected[i];
                }
            }

            includeNode.bound = calculateBound(includeNode, items, n, capacity);
            if (includeNode.bound > bestPriority)
            {
                insert(includeNode);
                printf("%-7d %-10.2f %-10.2f %-10.2f INCLUDE\n", includeNode.level, includeNode.weight, includeNode.priority, includeNode.bound);
            }
            else
            {
                nodesPruned++;
                printf("%-7d %-10.2f %-10.2f %-10.2f PRUNED\n", includeNode.level, includeNode.weight, includeNode.priority, includeNode.bound);
            }
        }
        else
        {
            nodesPruned++;
            printf("%-7d %-10.2f %-10.2f %-10s OVERWEIGHT\n", includeNode.level, includeNode.weight, includeNode.priority, "-");
        }
        excludeNode = current;
        excludeNode.level = next;
        excludeNode.selected[next] = 0;
        excludeNode.bound = calculateBound(excludeNode, items, n, capacity);
        if (excludeNode.bound > bestPriority)
        {
            insert(excludeNode);
            printf("%-7d %-10.2f %-10.2f %-10.2f EXCLUDE\n", excludeNode.level, excludeNode.weight, excludeNode.priority, excludeNode.bound);
        }
        else
        {
            nodesPruned++;
            printf("%-7d %-10.2f %-10.2f %-10.2f PRUNED\n", excludeNode.level, excludeNode.weight, excludeNode.priority, excludeNode.bound);
        }
    }

    printf("\n=================================================\n");
    printf("          OPTIMAL RELIEF PACKING\n");
    printf("=================================================\n");
    printf("\nSelected packages:\n\n");
    printf("%-8s %-10s %-10s\n", "Item", "Weight", "Priority");
    printf("---------------------------------\n");
    for (int i = 0; i < n; i++)
    {
        if (bestSelected[i])
        {
            printf("R%-7d %-10.2f %-10.2f\n",
                   items[i].id,
                   items[i].weight,
                   items[i].priority);
        }
    }
    printf("\nTotal Weight       = %.2f kg\n", bestWeight);
    printf("Total Priority     = %.2f\n", bestPriority);
    printf("Remaining Capacity = %.2f kg\n", capacity - bestWeight);
    printf("\nSEARCH STATISTICS\n");
    printf("---------------------------------\n");
    printf("Nodes Explored = %d\n", nodesExplored);
    printf("Nodes Pruned   = %d\n", nodesPruned);
    printf("=================================================\n");
}


int main()
{
    Item items[MAX];
    int n;
    float capacity;
    printf("=============================================\n");
    printf("      RELIEF PACKING - BRANCH AND BOUND\n");
    printf("=============================================\n");
    printf("\nEnter number of relief packages: ");
    scanf("%d", &n);
    if (n <= 0 || n > MAX)
    {
        printf("Invalid number of packages.\n");
        return 1;
    }
    printf("\nEnter weight and priority of each package:\n");
    for (int i = 0; i < n; i++)
    {
        items[i].id = i + 1;
        printf("\nPackage R%d\n", i + 1);
        printf("Weight: ");
        scanf("%f", &items[i].weight);
        printf("Priority: ");
        scanf("%f", &items[i].priority);
        if (items[i].weight <= 0 ||
            items[i].priority < 0)
        {
            printf("Invalid package data.\n");
            return 1;
        }
        items[i].ratio = items[i].priority / items[i].weight;
    }
    printf("\nEnter vehicle capacity: ");
    scanf("%f", &capacity);
    if (capacity <= 0)
    {
        printf("Invalid vehicle capacity.\n");
        return 1;
    }
    sortItems(items, n);
    printf("\nItems after sorting by priority/weight ratio:\n");
    printf("---------------------------------------------\n");
    printf("%-8s %-10s %-10s %-10s\n", "Item", "Weight", "Priority", "Ratio");
    for (int i = 0; i < n; i++)
    {
        printf("R%-7d %-10.2f %-10.2f %-10.2f\n", items[i].id, items[i].weight, items[i].priority, items[i].ratio);
    }
    branchAndBound(items, n, capacity);
    
return 0;
}