import numpy as np

class Layer:
    def forward(self, input):
        """
        Forward pass
        Args:
            input: input data
        Returns:
            output: output data
        """
        raise NotImplementedError
    
    def backward(self, grad_output):
        """
        Backward pass
        Args:
            grad_output: gradient of loss w.r.t. output of this layer
        Returns:
            grad_input: gradient of loss w.r.t. input of this layer
        """
        raise NotImplementedError


class Linear(Layer):
    def __init__(self, in_features, out_features):
        """
        Args:
            in_features: number of input features
            out_features: number of output features
        """
        std = np.sqrt(2.0 / in_features)
        self.W = np.random.randn(out_features, in_features) * std
        self.b = np.zeros(out_features)
        self.input = None
        self.grad_W = None
        self.grad_b = None

    def forward(self, input):
        """
        Forward pass: y = xW^T + b
        Args:
            input: (batch_size, in_features)
        Returns:
            output: (batch_size, out_features)
        """
        self.input = input
        output = np.dot(input, self.W.T) + self.b
        return output

    def backward(self, grad_output):
        """
        Backward pass
        Args:
            grad_output: (batch_size, out_features)
        Returns:
            grad_input: (batch_size, in_features)
        """
        grad_input = np.dot(grad_output, self.W)
        self.grad_W = np.dot(grad_output.T, self.input)
        self.grad_b = np.sum(grad_output, axis=0)
        return grad_input

class ReLU(Layer):
    def __init__(self):
        self.input = None

    def forward(self, input):
        """
        Forward pass:
        Args:
            input: any shape
        Returns:
            output: same shape as input
        """
        self.input = input
        output = np.maximum(0, input)
        return output

    def backward(self, grad_output):
        """
        Backward pass
        Args:
            grad_output: same shape as input
        Returns:
            grad_input: same shape as input
        """
        grad_input = grad_output * (self.input > 0)
        return grad_input 
    
def softmax(x):
    exp_x = np.exp(x - np.max(x, axis=1, keepdims=True))
    return exp_x / np.sum(exp_x, axis=1, keepdims=True) 

class SimpleNetwork:
    """
    Simple 3-layer neural network
    Architecture: Linear -> ReLU -> Linear -> ReLU -> Linear
    """
    def __init__(self, input_size, hidden_size1, hidden_size2, output_size):
        self.fc1 = Linear(input_size, hidden_size1)
        self.relu1 = ReLU()
        self.fc2 = Linear(hidden_size1, hidden_size2)
        self.relu2 = ReLU()
        self.fc3 = Linear(hidden_size2, output_size)
        self.layers = [self.fc1, self.relu1, self.fc2, self.relu2, self.fc3]

    def forward(self, x):
        out = x
        for layer in self.layers:
            out = layer.forward(out)
        return out

    def backward(self, grad_output):
        grad = grad_output
        for layer in reversed(self.layers):
            grad = layer.backward(grad)
        return grad
    
    
def train_with_gd(network, X_train, y_train, learning_rate, num_epochs):
    losses = []
    num_samples = X_train.shape[0]
    num_classes = 10

    for epoch in range(num_epochs):
        logits = network.forward(X_train)
        probs = softmax(logits)

        y_one_hot = np.zeros((num_samples, num_classes))
        y_one_hot[np.arange(num_samples), y_train] = 1

        loss = -np.sum(y_one_hot * np.log(probs + 1e-8)) / num_samples
        losses.append(loss)

        grad_output = (probs - y_one_hot) / num_samples
        network.backward(grad_output)

        for layer in network.layers:
            if isinstance(layer, Linear):
                layer.W -= learning_rate * layer.grad_W
                layer.b -= learning_rate * layer.grad_b

        if (epoch + 1) % 10 == 0:
            print(f"Epoch [{epoch+1}/{num_epochs}], Loss: {loss:.4f}")

    return losses


if __name__ == "__main__":
    np.random.seed(42)
    
    train_all = np.loadtxt('data/smallTrain.csv', dtype=int, delimiter=',')
    X_train = train_all[:, 1:]  
    y_train = train_all[:, 0]   

    valid_all = np.loadtxt('data/smallValidation.csv', dtype=int, delimiter=',')
    X_val = valid_all[:, 1:]   
    y_val = valid_all[:, 0]   

    input_size = 128    
    hidden_size1 = 128  
    hidden_size2 = 64   
    output_size = 10   

    network = SimpleNetwork(input_size, hidden_size1, hidden_size2, output_size)
    
    # Training parameters
    learning_rate = 0.1
    num_epochs = 100
    losses = train_with_gd(network, X_train, y_train, learning_rate, num_epochs)

    print(f"\nFinal Results:")

    # Training set evaluation
    logits_train = network.forward(X_train)
    probs_train = softmax(logits_train)
    predictions_train = np.argmax(probs_train, axis=1)
    accuracy_train = np.mean(predictions_train == y_train)

    print(f"  Training Loss: {losses[-1]:.4f}")
    print(f"  Training Accuracy: {accuracy_train:.2%}")

    # Validation set evaluation
    logits_val = network.forward(X_val)
    probs_val = softmax(logits_val)
    predictions_val = np.argmax(probs_val, axis=1)
    accuracy_val = np.mean(predictions_val == y_val)

    print(f"\n  Validation Accuracy: {accuracy_val:.2%}")